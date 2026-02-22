#ifndef PCB_H
#define PCB_H

typedef struct PCB {
    int pid;
    int start;
    int end;
    int pc; // Program Counter
    int job_time; // Total time required for the job
    int job_length_score; // Score used for SJF with aging
    struct PCB *next; // Pointer to the next PCB in the queue
} PCB;

PCB* make_pcb(int start, int end);

#endif
