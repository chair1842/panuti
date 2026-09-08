#ifndef _PANUTI_SYSCALLSF_H
#define _PANUTI_SYSCALLSF_H
#include "syscall.h"
#include "syscallno.h"
#include <stddef.h>
#include <stdint.h>

static inline int32_t panutisysf_write(int handle, const void* data, size_t size) {
	return panuti_syscall(SYSHANDLER_WRITE, (uint32_t)handle, (uint32_t)data, (uint32_t)size, 0);
}

static inline int32_t panutisysf_exit(uint32_t code) {
	return panuti_syscall(SYSHANDLER_EXIT, code, 0, 0, 0);
}

static inline int32_t panutisysf_open(const char* path) {
	return panuti_syscall(SYSHANDLER_OPEN, (uint32_t)path, 0, 0, 0);
}

static inline int32_t panutisysf_read(int handle, void* data, size_t size) {
	return panuti_syscall(SYSHANDLER_READ, (uint32_t)handle, (uint32_t)data, (uint32_t)size, 0);
}

static inline int32_t panutisysf_activate(int handle) {
	return panuti_syscall(SYSHANDLER_ACTIVATE, (uint32_t)handle, 0, 0, 0);
}

static inline int32_t panutisysf_close(int handle) {
	return panuti_syscall(SYSHANDLER_CLOSE, (uint32_t)handle, 0, 0, 0);
}

static inline int32_t panutisysf_mkdir(const char* path) {
	return panuti_syscall(SYSHANDLER_MKDIR, (uint32_t)path, 0, 0, 0);
}

static inline int32_t panutisysf_chdir(const char* path) {
	return panuti_syscall(SYSHANDLER_CHDIR, (uint32_t)path, 0, 0, 0);
}

static inline int32_t panutisysf_unlink(const char* path) {
	return panuti_syscall(SYSHANDLER_UNLINK, (uint32_t)path, 0, 0, 0);
}

static inline int32_t panutisysf_getpid(void) {
	return panuti_syscall(SYSHANDLER_GETPID, 0, 0, 0, 0);
}

static inline int32_t panutisysf_timesb(void) {
	return panuti_syscall(SYSHANDLER_TIMESB, 0, 0, 0, 0);
}

static inline int32_t panutisysf_getcwd(char* buf, size_t len) {
	return panuti_syscall(SYSHANDLER_GETCWD, (uint32_t)buf, (uint32_t)len, 0, 0);
}

#endif