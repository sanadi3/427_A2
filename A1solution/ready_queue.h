#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "pcb.h"

void ready_queue_add_to_tail(PCB *p);
void ready_queue_add_to_head(PCB *p); // Helper for some policies
PCB* ready_queue_pop_head();
PCB* ready_queue_pop_shortest(); // Helper for SJF policy
void ready_queue_insert_sorted(PCB *p); // insert PCB into the ready queue keeping it sorted by job_length_score (ascending)
void ready_queue_age_all(); // age by decreasing job_length_score by 1
PCB* ready_queue_peek_head(); // look at the head of the ready queue 
void ready_queue_print(); // Helper for debugging

#endif
