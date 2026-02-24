#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "scheduler.h"
#include "shellmemory.h"
#include "ready_queue.h"
#include "shell.h"

static int g_scheduler_active = 0;
static SchedulePolicy g_current_policy = POLICY_FCFS;
static int g_force_first_pid_once = -1;

// Multithreaded scheduler globals
static int mt_enabled = 0;
static pthread_t worker_threads[2];
static pthread_mutex_t rq_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rq_cond = PTHREAD_COND_INITIALIZER;
static int scheduler_quit = 0;

/*
 * 1.2.5 background-mode fix (for T_background):
 * Before this, SJF could pick a shorter user program before the batch-script process.
 * Assignment says batch script must run first once.
 * So we keep one "forced first PID" and pop that process first exactly one time.
 * After that, scheduling goes back to normal FCFS/SJF/RR/RR30/AGING behavior.
 */
static PCB* scheduler_pop_forced_first_if_any(void);
// Forward declaration for worker thread function (1.2.6)
static void* scheduler_worker_thread(void* arg);

static PCB* scheduler_pop_forced_first_if_any(void) {
    PCB *forced = NULL;
    if (g_force_first_pid_once < 0) {
        return NULL;
    }
    forced = ready_queue_pop_pid(g_force_first_pid_once);
    g_force_first_pid_once = -1;
    return forced;
}

// 1.2.3 helper. also reused by 1.2.4 aging loop
static int run_process_slice(PCB *current, int max_instructions, int last_error) {
    int executed = 0;

    while (current->pc <= current->end
           && (max_instructions < 0 || executed < max_instructions)) {
        char *line = mem_get_line(current->pc);
        if (line != NULL) {
            last_error = parseInput(line);
        }
        current->pc++;
        executed++;
    }

    return last_error;
}

static int scheduler_run_fcfs(void) {
    // 1.2.1 base scheduler behavior. 1.2.2 exec FCFS also lands here
    int last_error = 0;
    PCB *current = NULL;

    while ((current = scheduler_pop_forced_first_if_any()) != NULL
           || (current = ready_queue_pop_head()) != NULL) {
        last_error = run_process_slice(current, -1, last_error);

        mem_cleanup_script(current->start, current->end);
        free(current);
    }

    return last_error;
}

// 1.2.3: SJF sched. Always pick the process with the shortest job time (instructions)
static int scheduler_run_sjf(void) {
    int last_error = 0;
    PCB *current = NULL;

    while ((current = scheduler_pop_forced_first_if_any()) != NULL
           || (current = ready_queue_pop_shortest()) != NULL) {
        last_error = run_process_slice(current, -1, last_error);

        mem_cleanup_script(current->start, current->end);
        free(current);
    }

    return last_error;
}

// 1.2.3 + 1.2.5: RR core with configurable quantum (2 for RR, 30 for RR30)
static int scheduler_run_rr_quantum(int quantum) {
    int last_error = 0;
    PCB *current = NULL;

    while ((current = scheduler_pop_forced_first_if_any()) != NULL
           || (current = ready_queue_pop_head()) != NULL) {
        last_error = run_process_slice(current, quantum, last_error);

        if (current->pc > current->end) {
            mem_cleanup_script(current->start, current->end);
            free(current);
        } else {
            ready_queue_add_to_tail(current);
        }
    }

    return last_error;
}

// 1.2.4: AGING policy
static int scheduler_run_aging(void) {
    int last_error = 0;
    PCB *current = NULL;
    const int aging_quantum = 1;

    while ((current = scheduler_pop_forced_first_if_any()) != NULL
           || (current = ready_queue_pop_head()) != NULL) {
        last_error = run_process_slice(current, aging_quantum, last_error);

        if (current->pc > current->end) {
            mem_cleanup_script(current->start, current->end);
            free(current);
            continue;
        }

        // 1.2.4 exact rule: age waiting jobs, not the one that just ran
        ready_queue_age_all();

        // if current still lowest/tied-lowest, keep running. else reinsert sorted and promote
        PCB *next = ready_queue_peek_head();
        if (next == NULL || next->job_length_score >= current->job_length_score) {
            ready_queue_add_to_head(current);
        } else {
            ready_queue_insert_sorted(current);
        }
    }

    return last_error;
}

static int scheduler_run_mt_rr(int time_slice) {
    static int slice_arg[2];
    slice_arg[0] = time_slice;
    slice_arg[1] = time_slice;
    scheduler_quit = 0;
    pthread_create(&worker_threads[0], NULL, scheduler_worker_thread, &slice_arg[0]);
    pthread_create(&worker_threads[1], NULL, scheduler_worker_thread, &slice_arg[1]);
    // Wait for all jobs to finish
    while (1) {
        pthread_mutex_lock(&rq_mutex);
        int empty = (ready_queue_peek_head() == NULL);
        pthread_mutex_unlock(&rq_mutex);
        if (empty) break;
        usleep(10000); // sleep 10ms
    }
    scheduler_quit = 1;
    pthread_cond_broadcast(&rq_cond);
    pthread_join(worker_threads[0], NULL);
    pthread_join(worker_threads[1], NULL);
    return 0;
}

int scheduler_run(SchedulePolicy policy) {
    int rc = 1;

    if (mt_enabled && (policy == POLICY_RR || policy == POLICY_RR30)) {
        int slice = (policy == POLICY_RR) ? 2 : 30;
        return scheduler_run_mt_rr(slice);
    }
    // 1.2.5: avoid nested scheduler loops
    if (g_scheduler_active) {
        return 1;
    }
    g_scheduler_active = 1;
    g_current_policy = policy;

    // 1.2.2 policy dispatch entrypoint used by exec/source path
    switch (policy) {
    case POLICY_FCFS:
        rc = scheduler_run_fcfs();
        break;
    case POLICY_SJF:
        rc = scheduler_run_sjf();
        break;
    case POLICY_RR:
        rc = scheduler_run_rr_quantum(2);
        break;
    case POLICY_RR30:
        rc = scheduler_run_rr_quantum(30);
        break;
    case POLICY_AGING:
        rc = scheduler_run_aging();
        break;
    default:
        rc = 1;
        break;
    }

    g_scheduler_active = 0;
    return rc;
}

int scheduler_is_active(void) {
    return g_scheduler_active;
}

SchedulePolicy scheduler_get_current_policy(void) {
    return g_current_policy;
}

void scheduler_set_first_process_pid(int pid) {
    g_force_first_pid_once = pid;
}

void scheduler_enable_multithreaded() {
    mt_enabled = 1;
}

int scheduler_is_multithreaded() {
    return mt_enabled;
}

void scheduler_join_workers() {
    if (!mt_enabled) return;
    scheduler_quit = 1;
    pthread_cond_broadcast(&rq_cond);
    pthread_join(worker_threads[0], NULL);
    pthread_join(worker_threads[1], NULL);
}

// 1.2.6 Worker thread function for MT RR/RR30
static void* scheduler_worker_thread(void* arg) {
    int time_slice = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&rq_mutex);
        while (!scheduler_quit && ready_queue_peek_head() == NULL) {
            pthread_cond_wait(&rq_cond, &rq_mutex);
        }
        if (scheduler_quit) {
            pthread_mutex_unlock(&rq_mutex);
            break;
        }
        PCB *current = ready_queue_pop_head();
        pthread_mutex_unlock(&rq_mutex);
        if (!current) continue;
        run_process_slice(current, time_slice, 0);
        pthread_mutex_lock(&rq_mutex);
        if (current->pc > current->end) {
            mem_cleanup_script(current->start, current->end);
            free(current);
        } else {
            ready_queue_add_to_tail(current);
            pthread_cond_signal(&rq_cond);
        }
        pthread_mutex_unlock(&rq_mutex);
    }
    return NULL;
}
