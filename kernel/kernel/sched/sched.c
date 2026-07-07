#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <stddef.h>

static task_t* ready_queue = NULL;

void sched_add(task_t* task) {
    if (!ready_queue) {
        ready_queue = task;
        task->next = task;
    } else {
        task->next = ready_queue;
        ready_queue->next = task;
    }
}
