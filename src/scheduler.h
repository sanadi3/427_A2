#ifndef SCHEDULER_H
#define SCHEDULER_H

typedef enum {
    POLICY_FCFS = 0,
    POLICY_SJF,
    POLICY_RR,
    POLICY_RR30,
    POLICY_AGING
} SchedulePolicy;

int scheduler_run(SchedulePolicy policy);
int scheduler_is_active(void);
SchedulePolicy scheduler_get_current_policy(void);
void scheduler_set_first_process_pid(int pid);

#endif
