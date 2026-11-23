/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_EVENT_H
#define _PANUTI_EVENT_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*ev_callback_t)(int fd, void* arg);

int ev_add(int fd, ev_callback_t cb, void* arg);

int ev_del(int fd);

int ev_loop(int32_t timeout_cs);

#if defined(__cplusplus)
}
#endif

#endif
