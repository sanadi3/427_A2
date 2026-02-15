#include <stdlib.h>
#include <stdio.h>

#include "scheduler.h"
#include "shellmemory.h"
#include "ready_queue.h"
#include "shell.h"

static int scheduler_run_fcfs(void) {
    int last_error = 0;
    PCB *current = NULL;

    while ((current = ready_queue_pop_head()) != NULL) {
        while (current->pc <= current->end) {
            char *line = mem_get_line(current->pc);
            if (line != NULL) {
                last_error = parseInput(line);
            }
            current->pc++;
        }

        mem_cleanup_script(current->start, current->end);
        free(current);
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
    // FCFS executes normally; other policies are placeholders for now.
    switch (policy) {
    case POLICY_FCFS:
        return scheduler_run_fcfs();
    case POLICY_SJF:
    case POLICY_RR:
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
