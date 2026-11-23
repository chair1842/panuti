/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/handle.h>
#include <panuti/syscall/syscallsf.h>

int hwait(const int* fds, uint32_t n, int32_t timeout_cs, uint32_t* which) {
	return panutisysf_hwait(fds, n, timeout_cs, which);
}
