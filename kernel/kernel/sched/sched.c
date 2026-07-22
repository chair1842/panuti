#include "kernel/klog.h"
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/kpanic.h>

static task_t* ready_queue = NULL;
static task_t* current = NULL;
static bool sched_initialized = false;

void sched_add(task_t* task) {
    if (!ready_queue) {
        ready_queue = task;
        task->next = task;
    } else {
        task->next = ready_queue->next;
        ready_queue->next = task;
        klog(KLOG_INFO, "sched_add: ready_queue (%x) next updated to %x\n", ready_queue->pid, ready_queue->next->pid);
    }
    
    klog(KLOG_INFO, "sched_add: next of task %x is %x\n", task->pid, task->next->pid);
}

void sched_schedule(void) {
    if (!sched_initialized) {
        return;
    }
    
    if (!current) {
        current = ready_queue;
        current->state = TASK_RUNNING;
        return;
    }

    task_t* prev = current;

    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
    }

    task_t* next = prev->next;
    while (next->state != TASK_READY) {
        if (next == prev) {
            if (prev->state == TASK_READY) {
                next = prev; 
                break;
            }
            
            kpanic("sched_schedule: no runnable tasks");
        }
        
        next = next->next;
    }

    current = next;
    current->state = TASK_RUNNING;

    if (prev != current) {
        klog(KLOG_INFO, "sched_schedule: switching from %p to %p\n", prev, current);
        task_activate(current);
        task_switch_to(prev, current);
    }
}

void sched_init(void) {
    if (!ready_queue) {
        return;
    }
    
    current = ready_queue;
    current->state = TASK_RUNNING;
    task_activate(current);
    sched_initialized = true;
    
    task_t dummy; // to be thrown away
    task_switch_to(&dummy, current);
}

task_t* sched_current(void) {
    return current;
}

void task_block(task_t* t) {
    t->state = TASK_BLOCKED;
    if (t == current) {
        sched_schedule();
    }
}

void task_wake(task_t* t) {
    if (t->state == TASK_BLOCKED) {
        t->state = TASK_READY;
    }
}
