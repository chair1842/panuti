#ifndef _PANUTI_SYSCALLSF_H
#define _PANUTI_SYSCALLSF_H
#include "syscall.h"
#include "syscallno.h"
#include <stddef.h>

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

static inline int32_t panutisysf_wait(int handle) {
	return panuti_syscall(SYSHANDLER_WAIT, (uint32_t)handle, 0, 0, 0);
}

static inline int32_t panutisysf_close(int handle) {
	return panuti_syscall(SYSHANDLER_CLOSE, (uint32_t)handle, 0, 0, 0);
}

static inline int32_t panutisysf_mkdir(const char* path) {
	return panuti_syscall(SYSHANDLER_MKDIR, (uint32_t)path, 0, 0, 0);
}

static inline int32_t panutisysf_regfind(const char* path) {
	return panuti_syscall(SYSHANDLER_REGFIND, (uint32_t)path, 0, 0, 0);
}

static inline int32_t panutisysf_regroot(void) {
	return panuti_syscall(SYSHANDLER_REGROOT, 0, 0, 0, 0);
}


#endif