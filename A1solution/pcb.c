#include <stdlib.h>
#include <stdio.h>
#include "pcb.h"

int pid_counter = 0; // global pid counter

PCB* make_pcb(int start, int end) {
    PCB* new_pcb = (PCB*)malloc(sizeof(PCB));
    if (new_pcb == NULL) {
        fprintf(stderr, "Memory allocation failed for PCB\n");
        return NULL;
    }
    new_pcb->pid = ++pid_counter; //pre increment to start from 1
    new_pcb->start = start;
    new_pcb->end = end;
    new_pcb->pc = start; // Initialize program counter to the start of the job
    new_pcb->job_time = (end - start+1); // Initialize job time to number of instructions
    new_pcb->job_length_score = new_pcb->job_time; // initialize score for aging policy
    new_pcb->next = NULL; // Initialize next pointer to NULL
    return new_pcb;
}
