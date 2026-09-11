#ifndef _KERNEL_SYSCALL_HANDLERS_H
#define _KERNEL_SYSCALL_HANDLERS_H

#include <stdint.h>

// write to a handle
int32_t syshandler_write(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// exit the current process
int32_t syshandler_exit(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// open a handle
int32_t syshandler_open(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// read from a handle
int32_t syshandler_read(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// activate a handle
int32_t syshandler_activate(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// close a handle
int32_t syshandler_close(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// mkdir in registry
int32_t syshandler_mkdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// change the current working directory
int32_t syshandler_chdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// unlink a name from a directory
int32_t syshandler_unlink(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// get the pid of the currently running process
int32_t syshandler_getpid(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// get the time in centiseconds scince boot
int32_t syshandler_timesb(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// get the current working directory
int32_t syshandler_getcwd(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// give up the cpu for a turn
int32_t syshandler_yield(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// move a name somewhere else
int32_t syshandler_rename(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// add a second name that points at the same inode
int32_t syshandler_link(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// mount a filesystem from a block device onto a mountpoint
int32_t syshandler_mount(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// unmount a filesystem from a mountpoint
int32_t syshandler_unmount(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// create an anonymous pipe
int32_t syshandler_pipe_create(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

#endif