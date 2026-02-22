#include <stdlib.h>
#include <stdio.h>

#include "scheduler.h"
#include "shellmemory.h"
#include "ready_queue.h"
#include "shell.h"

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

    while ((current = ready_queue_pop_head()) != NULL) {
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

    while ((current = ready_queue_pop_shortest()) != NULL) {
        last_error = run_process_slice(current, -1, last_error);

        mem_cleanup_script(current->start, current->end);
        free(current);
    }

    return last_error;
}

// 1.2.3: RR sched. If a process doesn't finish within its time slice, it goes back to the end of the ready queue.
static int scheduler_run_rr(void) {
    int last_error = 0;
    PCB *current = NULL;
    const int rr_quantum = 2; // time slice

    while ((current = ready_queue_pop_head()) != NULL) {
        last_error = run_process_slice(current, rr_quantum, last_error);

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

    while ((current = ready_queue_pop_head()) != NULL) {
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

int scheduler_run(SchedulePolicy policy) {
    // 1.2.2 policy dispatch entrypoint used by exec/source path
    switch (policy) {
    case POLICY_FCFS:
        return scheduler_run_fcfs();
    case POLICY_SJF:
        return scheduler_run_sjf();
    case POLICY_RR:
        return scheduler_run_rr();
    case POLICY_AGING:
        return scheduler_run_aging();
    default:
        return 1;
    }
}
