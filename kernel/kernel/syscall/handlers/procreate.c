/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <panuti/syscall/procreate.h>
#include <stdint.h>

int32_t syshandler_procreate(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const procreate_args_t* user_args = (const procreate_args_t*)a1;

	if (!kernel_usrptr_verify(user_args, sizeof(procreate_args_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	procreate_args_t args = *user_args;

	size_t path_len = kernel_user_strlen(args.path);
	if (path_len == (size_t)-1 || !kernel_usrptr_verify(args.path, path_len + 1)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (args.argc < 0 || args.argc > MAX_ARGV_COUNT) {
		return PANUTIERRNO_PLAINERR;
	}
	
	if (args.argc > 0) {
		if (!kernel_usrptr_verify(args.argv, sizeof(char*) * (size_t)args.argc)) {
			return PANUTIERRNO_INVALIDADDR;
		}
		
		for (int i = 0; i < args.argc; i++) {
			size_t arg_len = kernel_user_strlen(args.argv[i]);
			if (arg_len == (size_t)-1 || !kernel_usrptr_verify(args.argv[i], arg_len + 1)) {
				return PANUTIERRNO_INVALIDADDR;
			}
		}
	}

	if (args.no_in_streams < 0 || args.no_in_streams > MAX_STREAMS) {
		return PANUTIERRNO_PLAINERR;
	}
	
	if (
		args.no_in_streams > 0 
		&& !kernel_usrptr_verify(args.in_streams, sizeof(int) * (size_t)args.no_in_streams)
	) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (args.no_out_streams < 0 || args.no_out_streams > MAX_STREAMS) {
		return PANUTIERRNO_PLAINERR;
	}
	
	if (
		args.no_out_streams > 0 
		&& !kernel_usrptr_verify(args.out_streams, sizeof(int) * (size_t)args.no_out_streams)
	) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* caller = sched_current();

	pid_t pid = task_procreate(caller, &args);
	return (int32_t)pid;
}