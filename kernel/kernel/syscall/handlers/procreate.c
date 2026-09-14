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

	if (!kernel_is_user_ptr(user_args)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	procreate_args_t args = *user_args;

	if (!kernel_is_user_ptr(args.path)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (args.argc < 0 || args.argc > MAX_ARGV_COUNT) {
		return PANUTIERRNO_PLAINERR;
	}
	
	if (args.argc > 0) {
		if (!kernel_is_user_range(args.argv, sizeof(char*) * (size_t)args.argc)) {
			return PANUTIERRNO_INVALIDADDR;
		}
		
		for (int i = 0; i < args.argc; i++) {
			if (!kernel_is_user_ptr(args.argv[i])) {
				return PANUTIERRNO_INVALIDADDR;
			}
		}
	}

	if (args.no_in_streams < 0 || args.no_in_streams > MAX_STREAMS) {
		return PANUTIERRNO_PLAINERR;
	}
	
	if (
		args.no_in_streams > 0 
		&& !kernel_is_user_range(args.in_streams, sizeof(int) * (size_t)args.no_in_streams)
	) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (args.no_out_streams < 0 || args.no_out_streams > MAX_STREAMS) {
		return PANUTIERRNO_PLAINERR;
	}
	
	if (
		args.no_out_streams > 0 
		&& !kernel_is_user_range(args.out_streams, sizeof(int) * (size_t)args.no_out_streams)
	) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* caller = sched_current();

	pid_t pid = task_procreate(caller, &args);
	return (int32_t)pid;
}