#include <stdio.h>
#include "ready_queue.h"

// Global pointers to head and tail 
PCB *head = NULL;
PCB *tail = NULL;

// Add a PCB to the end of the queue
void ready_queue_add_to_tail(PCB *p) {
    if (!p) return;
    p->next = NULL;

    if (head == NULL) {
        head = p;
        tail = p;
    } else {
        tail->next = p;
        tail = p;
    }
}

// Add a PCB to the front of the queue
void ready_queue_add_to_head(PCB *p) {
    if (!p) return;

    if (head == NULL) {
        head = p;
        tail = p;
        p->next = NULL;
    } else {
        p->next = head;
        head = p;
    }
}

// Remove and return the PCB at the head of the queue 
PCB* ready_queue_pop_head() {
    if (head == NULL) return NULL;

    PCB *temp = head;
    head = head->next;
    
    // If queue becomes empty, reset tail
    if (head == NULL) {
        tail = NULL;
    }
    
    temp->next = NULL; // Isolate the popped PCB
    return temp;
}

// print queue for debugging
void ready_queue_print() {
    PCB *curr = head;
    printf("Ready Queue: ");
    while (curr != NULL) {
        printf("[PID:%d] -> ", curr->pid);
        curr = curr->next;
    }
    printf("NULL\n");
}