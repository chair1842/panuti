/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

nstreams_t nstream() {
	int out[2];
	panutisysf_nstream(out);
	return (nstreams_t){ out[0], out[1] };
}