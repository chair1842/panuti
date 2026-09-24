#ifndef _PANUTI_PROCESS_H
#define _PANUTI_PROCESS_H

#include <sys/types.h>

// creates a process
pid_t procreate(
	const char* path,
	char** argv, int argc,
	int* in_streams, int no_in_streams,
	int* out_streams, int no_out_streams
);

// block until the waited-on process exits and then output the exit code to ec_out
int wait(pid_t pid, int* ec_out);

#endif