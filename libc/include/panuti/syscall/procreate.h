/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_SYSCALL_PROCREATE_H
#define _PANUTI_SYSCALL_PROCREATE_H

#define MAX_ARGV_COUNT 32

typedef struct procreate_args {
	const char* path;
	
	char** argv;
	int argc;

	int* in_streams;
	int no_in_streams;

	int* out_streams;
	int no_out_streams;
} procreate_args_t;

#endif