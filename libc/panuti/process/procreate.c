/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/process.h>
#include <panuti/syscall/procreate.h>
#include <panuti/syscall/syscallsf.h>

pid_t procreate(
	const char* path,
	char** argv, int argc,
	int* in_streams, int no_in_streams,
	int* out_streams, int no_out_streams
) {
	procreate_args_t args = {
		.path = path,
		.argv = argv, .argc = argc,
		.in_streams = in_streams,
		.no_in_streams = no_in_streams,
		.out_streams = out_streams,
		.no_out_streams = no_out_streams
	};

	return panutisysf_procreate(&args);
}