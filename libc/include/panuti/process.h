/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_PROCESS_H
#define _PANUTI_PROCESS_H

#include <stdint.h>
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

// creates a process
pid_t procreate(
	const char* path,
	char** argv, int argc,
	int* in_streams, int no_in_streams,
	int* out_streams, int no_out_streams
);

// block until the waited-on process exits and then output the exit code to ec_out
int wait(pid_t pid, int* ec_out);

// block until any one of the pids exits, then output its exit code and pid
int wait_many(const pid_t* pids, uint32_t count, int* ec_out, pid_t* exit_pid);

// like wait() but return PANUTIERRNO_BUSY instead of blocking
int waitp(pid_t pid, int* ec_out);

// polling form of wait_many(); returns PANUTIERRNO_BUSY if none have exited
int waitp_many(const pid_t* pids, uint32_t count, int* ec_out, pid_t* exit_pid);

// sleep for cs amount of centiseconds
int sleep(int cs);

// request that the process with pid terminate; 0 on success
int kill(pid_t pid);

#if defined(__cplusplus)
}
#endif

#endif
