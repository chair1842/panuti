/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_SYSCALL_HANDLERS_H
#define _KERNEL_SYSCALL_HANDLERS_H

#include <stdint.h>
#include <kernel/mem/usr.h>

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

// output the number of streams
int32_t syshandler_nstream(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// read from the in streams
int32_t syshandler_stream_read(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// write to the out streams
int32_t syshandler_stream_write(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// create a process
int32_t syshandler_procreate(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// wait for a process to exit
int32_t syshandler_wait(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// read dirents from a directory
int32_t syshandler_readdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// describe a path as a dirent
int32_t syshandler_stat(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// report whether a path resolves to something
int32_t syshandler_nexist(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// map a run of anonymous pages and return its base address, or a negative error
int32_t syshandler_mmapan(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// release a run of anonymous pages
int32_t syshandler_munmap(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// resize a handle
int32_t syshandler_resize(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// create a new empty file
int32_t syshandler_mkfile(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// change the read/write handle offset
int32_t syshandler_seek(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

#endif