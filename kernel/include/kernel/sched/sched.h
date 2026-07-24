#ifndef _KERNEL_SCHED_SCHED_H
#define _KERNEL_SCHED_SCHED_H

#include <kernel/sched/task.h>

void sched_add(task_t* task);
void sched_schedule(void);
void sched_init(void);
task_t* sched_current(void);
void task_wake(task_t* task);
void task_block(task_t* task);

#endif