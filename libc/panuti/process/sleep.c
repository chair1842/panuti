/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/process.h>
#include <panuti/syscall/syscallsf.h>

int sleep(int cs) {
	return panutisysf_sleep(cs);
}