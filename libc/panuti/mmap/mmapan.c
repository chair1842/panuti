/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/mmap.h>
#include <panuti/syscall/syscallsf.h>

int32_t mmapan(size_t len, int prot, void* addr_hint) {
	return panutisysf_mmapan(len, prot, addr_hint);
}
