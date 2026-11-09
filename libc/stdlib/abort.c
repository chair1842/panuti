/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>

#include <panuti/syscall/syscallsf.h>

#include <panuti/errno.h>

__attribute__((__noreturn__))
void abort(void) {
	panutisysf_exit(PANUTIERRNO_PLAINERR);
	while (1) { }
	__builtin_unreachable();
}
