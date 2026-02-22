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

// insert PCB into the ready queue and keeping it sorted by job_length_score (ascending)
void ready_queue_insert_sorted(PCB *p) {
    if (!p) return;
    p->next = NULL;
    if (head == NULL) {
        head = p;
        tail = p;
        return;
    }
    // if new node, should be new head
    if (p->job_length_score <= head->job_length_score) {
        p->next = head;
        head = p;
        return;
    }
    PCB *curr = head;
    while (curr->next != NULL && curr->next->job_length_score <= p->job_length_score) {
        curr = curr->next;
    }
    p->next = curr->next;
    curr->next = p;
    if (p->next == NULL) tail = p;
}

// age by decreasing job length score by 1
void ready_queue_age_all() {
    PCB *curr = head;
    while (curr != NULL) {
        if (curr->job_length_score > 0) curr->job_length_score--;
        curr = curr->next;
    }
}

// look at the head
PCB* ready_queue_peek_head() {
    return head;
}


// 1.2.3: Remove and return the PCB with the shortest job time from the queue
PCB* ready_queue_pop_shortest() {
    if (head == NULL) return NULL;

    PCB *prev = NULL;
    PCB *curr = head;
    PCB *min_prev = NULL;
    PCB *min_node = head;

    while (curr != NULL) {
        if (curr->job_time < min_node->job_time) {
            min_node = curr;
            min_prev = prev;
        }
        prev = curr;
        curr = curr->next;
    }

    if (min_prev == NULL) {
        head = min_node->next;
    } else {
        min_prev->next = min_node->next;
    }

    if (tail == min_node) {
        tail = min_prev;
    }

    min_node->next = NULL;
    return min_node;
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
