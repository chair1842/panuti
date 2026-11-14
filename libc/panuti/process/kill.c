/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/process.h>
#include <panuti/syscall/syscallsf.h>

int kill(pid_t pid) {
	return panutisysf_kill(pid);
}
