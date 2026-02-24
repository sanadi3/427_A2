#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

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
static int mt_workers_started = 0;
static int mt_active_workers = 0;
static int mt_time_slice = 30;

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
            if (last_error == -2) {
                break;
            }
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
    int rc0 = 0, rc1 = 0;

    mt_time_slice = time_slice;

    pthread_mutex_lock(&rq_mutex);
    if (!mt_workers_started) {
        scheduler_quit = 0;
        mt_active_workers = 0;
        mt_workers_started = 1;
        pthread_mutex_unlock(&rq_mutex);

        rc0 = pthread_create(&worker_threads[0], NULL, scheduler_worker_thread, NULL);
        rc1 = pthread_create(&worker_threads[1], NULL, scheduler_worker_thread, NULL);

        if (rc0 != 0 || rc1 != 0) {
            if (rc0 == 0) {
                pthread_cancel(worker_threads[0]);
                pthread_join(worker_threads[0], NULL);
            }
            if (rc1 == 0) {
                pthread_cancel(worker_threads[1]);
                pthread_join(worker_threads[1], NULL);
            }
            pthread_mutex_lock(&rq_mutex);
            mt_workers_started = 0;
            scheduler_quit = 0;
            pthread_mutex_unlock(&rq_mutex);
            g_scheduler_active = 0;
            return 1;
        }
    } else {
        pthread_mutex_unlock(&rq_mutex);
    }

    pthread_mutex_lock(&rq_mutex);
    pthread_cond_broadcast(&rq_cond);
    pthread_mutex_unlock(&rq_mutex);
    return 0;
}

int scheduler_run(SchedulePolicy policy) {
    int rc = 1;

    // 1.2.5: avoid nested scheduler loops
    if (g_scheduler_active) {
        return 1;
    }
    g_scheduler_active = 1;
    g_current_policy = policy;

    if (mt_enabled && (policy == POLICY_RR || policy == POLICY_RR30)) {
        int slice = (policy == POLICY_RR) ? 2 : 30;
        return scheduler_run_mt_rr(slice);
    }

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

void scheduler_set_multithreaded(int enabled) {
    mt_enabled = enabled ? 1 : 0;
}

int scheduler_is_multithreaded(void) {
    return mt_enabled;
}

int scheduler_is_worker_thread(void) {
    pthread_t self;

    if (!mt_workers_started) {
        return 0;
    }

    self = pthread_self();
    if (pthread_equal(self, worker_threads[0]) || pthread_equal(self, worker_threads[1])) {
        return 1;
    }
    return 0;
}

void scheduler_join_workers() {
    if (!mt_workers_started) {
        g_scheduler_active = 0;
        return;
    }

    pthread_mutex_lock(&rq_mutex);
    while (ready_queue_peek_head() != NULL || mt_active_workers > 0) {
        pthread_cond_wait(&rq_cond, &rq_mutex);
    }
    scheduler_quit = 1;
    pthread_cond_broadcast(&rq_cond);
    pthread_mutex_unlock(&rq_mutex);

    pthread_join(worker_threads[0], NULL);
    pthread_join(worker_threads[1], NULL);

    pthread_mutex_lock(&rq_mutex);
    mt_workers_started = 0;
    scheduler_quit = 0;
    pthread_mutex_unlock(&rq_mutex);

    g_scheduler_active = 0;
}

void scheduler_ready_queue_lock(void) {
    pthread_mutex_lock(&rq_mutex);
}

void scheduler_ready_queue_unlock(void) {
    pthread_mutex_unlock(&rq_mutex);
}

void scheduler_ready_queue_signal(void) {
    pthread_cond_broadcast(&rq_cond);
}

// 1.2.6 Worker thread function for MT RR/RR30
static void* scheduler_worker_thread(void* arg) {
    (void)arg;

    while (1) {
        PCB *current = NULL;
        int last_error = 0;

        pthread_mutex_lock(&rq_mutex);
        while (!scheduler_quit && ready_queue_peek_head() == NULL) {
            pthread_cond_wait(&rq_cond, &rq_mutex);
        }
        if (scheduler_quit && ready_queue_peek_head() == NULL) {
            pthread_mutex_unlock(&rq_mutex);
            break;
        }

        current = scheduler_pop_forced_first_if_any();
        if (current == NULL) {
            current = ready_queue_pop_head();
        }
        if (current != NULL) {
            mt_active_workers++;
        }
        pthread_mutex_unlock(&rq_mutex);

        if (!current) {
            continue;
        }

        last_error = run_process_slice(current, mt_time_slice, 0);

        pthread_mutex_lock(&rq_mutex);
        if (last_error == -2) {
            mem_cleanup_script(current->start, current->end);
            free(current);
            mt_active_workers--;
            pthread_cond_broadcast(&rq_cond);
            pthread_mutex_unlock(&rq_mutex);
            break;
        } else if (current->pc > current->end) {
            mem_cleanup_script(current->start, current->end);
            free(current);
        } else {
            ready_queue_add_to_tail(current);
        }
        mt_active_workers--;
        pthread_cond_broadcast(&rq_cond);
        pthread_mutex_unlock(&rq_mutex);
    }
    return NULL;
}
