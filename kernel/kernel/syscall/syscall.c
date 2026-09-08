#include <kernel/syscall/syscall.h>
#include <panuti/errno.h>
#include <panuti/syscall/syscallno.h>
#include <kernel/syscall/handlers.h>

syscall_handler_t syscall_handlers[256] = {
	[SYSHANDLER_WRITE] = syshandler_write,
	[SYSHANDLER_EXIT] = syshandler_exit,
	[SYSHANDLER_OPEN] = syshandler_open,
	[SYSHANDLER_READ] = syshandler_read,
	[SYSHANDLER_ACTIVATE] = syshandler_activate,
	[SYSHANDLER_CLOSE] = syshandler_close,
	[SYSHANDLER_MKDIR] = syshandler_mkdir,
	[SYSHANDLER_CHDIR] = syshandler_chdir,
	[SYSHANDLER_UNLINK] = syshandler_unlink,
	[SYSHANDLER_GETPID] = syshandler_getpid,
	[SYSHANDLER_TIMESB] = syshandler_timesb,
	[SYSHANDLER_GETCWD] = syshandler_getcwd,
};

int32_t syscall_dispatch(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	if (num >= 256 || !syscall_handlers[num]) {
		return PANUTIERRNO_INVALIDSYSCALL;
	}

	return syscall_handlers[num](a1, a2, a3, a4);
}