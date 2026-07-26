#include <kernel/handle/handle.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include "handlers.h"
#include <kernel/handle/point.h>

static int wait_scan(task_t* task, int* fds, size_t count, int* fired_fd) {
    for (size_t i = 0; i < count; i++) {
        int fd = fds[i];

        if (fd < 0 || fd >= MAX_HANDLES) {
            continue;
        }

        handle_t* h = &task->handles[fd];

        if (!h->ops || !h->ops->ready) {
            continue;
        }

        if (!h->ops->ready(h->impl)) {
            continue;
        }

        if (h->type == INODE_POINT) {
            point_t* point = h->impl;
            point->pending = false;
        }

        *fired_fd = fd;
        return 1;
    }

    return 0;
}

int32_t syshandler_wait(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a4;

    int* fds = (int*)a1;
    size_t count = (size_t)a2;
    int* fired_fd = (int*)a3;

    if (!is_user_ptr(fds)) {
        return PANUTIERRNO_INVALIDADDR;
    }

    if (!is_user_ptr(fired_fd)) {
        return PANUTIERRNO_INVALIDADDR;
    }

    task_t* current = sched_current();

    while (1) {
        if (wait_scan(current, fds, count, fired_fd)) {
            return PANUTIERRNO_PLAINSUCCESS;
        }

        current->state = TASK_BLOCKED;

        sched_schedule();
    }
}
