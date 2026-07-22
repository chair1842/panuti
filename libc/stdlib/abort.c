#include <stdio.h>
#include <stdlib.h>

#if defined (__is_libk)
#include <kernel/kpanic.h>
#else
#include <panuti/syscall/syscallsf.h>
#endif

#include <panuti/errno.h>

__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
	kpanic("abort() has been called (please use kpanic directly instead of abort)\n");
#else
	panutisysf_exit(PANUTIERRNO_PLAINERR);
#endif
	while (1) { }
	__builtin_unreachable();
}
