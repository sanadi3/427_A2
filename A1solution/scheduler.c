#include <stdlib.h>

#include "scheduler.h"
#include "shellmemory.h"
#include "ready_queue.h"
#include "shell.h"

int scheduler_run_fcfs(void) {
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
