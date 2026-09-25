/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

void nstream(int out[2]) {
	panutisysf_nstream(out);
}