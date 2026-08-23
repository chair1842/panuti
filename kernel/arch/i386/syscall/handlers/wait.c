#include <kernel/handle/handle.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include "handlers.h"

// returns: 1 = found ready fd (written to *fired_fd), 0 = none ready, -1 = no fds support waiting
static int wait_scan(task_t* task, int* fds, size_t count, int* fired_fd) {
    int any_supported = 0;

    for (size_t i = 0; i < count; i++) {
        int fd = fds[i];

        if (fd < 0 || fd >= MAX_HANDLES) {
            continue;
        }

        handle_t* h = &task->handles[fd];

        if (!h->ops || !h->ops->ready) {
            continue;
        }

        int rdy = h->ops->ready(h->impl);
        if (rdy < 0) {
            continue; // unsupported
        }

        any_supported = 1;

        if (rdy > 0) {
            *fired_fd = fd;
            return 1;
        }
    }

    return any_supported ? 0 : -1;
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
        int result = wait_scan(current, fds, count, fired_fd);

        if (result > 0) {
            return PANUTIERRNO_PLAINSUCCESS;
        }

        if (result < 0) {
            return PANUTIERRNO_UNSUPPORTEDOP;
        }

        current->state = TASK_BLOCKED;
        sched_schedule();
    }
}
