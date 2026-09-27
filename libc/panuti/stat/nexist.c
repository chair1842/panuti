/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>

bool nexist(const char* path) {
	// the syscall answers 1 or 0, and anything at 0x80000000 or above is an
	// error code, so only an exact 1 counts as "it is there"
	return panutisysf_nexist(path) == 1;
}
