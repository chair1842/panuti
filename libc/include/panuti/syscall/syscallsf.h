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

static inline int32_t panutisysf_wait(int des) {
	return panuti_syscall(SYSHANDLER_WAIT, (uint32_t)des, 0, 0, 0);
}

static inline int32_t panutisysf_activate(int des) {
	return panuti_syscall(SYSHANDLER_ACTIVATE, (uint32_t)des, 0, 0, 0);
}

static inline int32_t panutisysf_close(int des) {
	return panuti_syscall(SYSHANDLER_CLOSE, (uint32_t)des, 0, 0, 0);
}

static inline int32_t panutisysf_point_create(const char* path) {
	return panuti_syscall(SYSHANDLER_POINT_CREATE, (uint32_t)path, 0, 0, 0);
}

static inline int32_t panutisysf_getpid(void) {
	return panuti_syscall(SYSHANDLER_GETPID, 0, 0, 0, 0);
}

#endif