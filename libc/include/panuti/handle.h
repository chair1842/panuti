/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_HANDLE_H
#define _PANUTI_HANDLE_H

#include <stddef.h>
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

// write len bytes of data to the fd
int handle_write(int fd, const void* data, size_t len);

// read len bytes from fd to data_out
int handle_read(int fd, void* data_out, size_t len);

// open a path and receive a fd
int handle_open(const char* path);

// close an fd
int handle_close(int fd);

// change the current pos of the handle
int handle_seek(int fd, off_t offset, int whence);

#if defined(__cplusplus)
}
#endif

#endif
