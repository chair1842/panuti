/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/registry.h>
#include <panuti/syscall/syscallsf.h>

int mkfile(const char* path) {
	return panutisysf_mkfile(path);
}