#ifndef _PANUTI_SYSCALLSF_H
#define _PANUTI_SYSCALLSF_H
#include "syscall.h"
#include "syscallno.h"

static inline int32_t panutisysf_vga(char c) {
	return panuti_syscall(SYSHANDLER_VGA, (uint32_t)c, 0, 0, 0);
}

static inline int32_t panutisysf_exit(uint32_t code) {
	return panuti_syscall(SYSHANDLER_EXIT, code, 0, 0, 0);
}

#endif