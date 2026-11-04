/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>

int settmsp(const char* path, inode_timestamp_t* timestamp, unsigned int mask) {
	return panutisysf_settmsp(path, timestamp, mask);
}
