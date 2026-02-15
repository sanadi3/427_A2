#include <stdlib.h>
#include <stdio.h>

#include "scheduler.h"
#include "shellmemory.h"
#include "ready_queue.h"
#include "shell.h"

// 1.2.3: Helper function to run a process for a given number of instructions (or until completion)
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

static void scheduler_cleanup_ready_queue(void) {
    PCB *current = NULL;

    while ((current = ready_queue_pop_head()) != NULL) {
        mem_cleanup_script(current->start, current->end);
        free(current);
    }
}

int scheduler_run(SchedulePolicy policy) {
    // A2 1.2.2: scheduler policy dispatch.
    // FCFS/SJF/RR execute normally; AGING is a placeholder for now.
    switch (policy) {
    case POLICY_FCFS:
        return scheduler_run_fcfs();
    case POLICY_SJF:
        return scheduler_run_sjf();
    case POLICY_RR:
        return scheduler_run_rr();
    case POLICY_AGING:
        // A2 1.2.2: Since code/PCBs are already loaded through the shared path,
        // clean up queued processes for unimplemented policies to keep state clean.
        scheduler_cleanup_ready_queue();
        printf("Scheduling policy not implemented\n");
        return 1;
    default:
        return 1;
    }
}
