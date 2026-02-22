#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "pcb.h"

void ready_queue_add_to_tail(PCB *p);
void ready_queue_add_to_head(PCB *p); // Helper for some policies
PCB* ready_queue_pop_head();
PCB* ready_queue_pop_shortest(); // Helper for SJF policy
void ready_queue_insert_sorted(PCB *p); // Insert by job_length_score ascending
void ready_queue_age_all(void); // Decrease job_length_score by 1 (min 0) for all queued jobs
PCB* ready_queue_peek_head(void); // Observe head without dequeue
void ready_queue_print(); // Helper for debugging

#endif
