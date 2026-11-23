/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_PROC_H
#define _KERNEL_PROC_H

#include <kernel/sched/task.h>

void proc_init(void);
void proc_task_add(task_t* task);
void proc_task_remove(pid_t pid);

#endif
