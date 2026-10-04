/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/registry.h>
#include <panuti/syscall/syscallsf.h>

int32_t mkdir(const char* path) {
	return panutisysf_mkdir(path);
}
