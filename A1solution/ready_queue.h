#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "pcb.h"

void ready_queue_add_to_tail(PCB *p);
void ready_queue_add_to_head(PCB *p); // Helper for some policies
PCB* ready_queue_pop_head();
void ready_queue_print(); // Helper for debugging

#endif