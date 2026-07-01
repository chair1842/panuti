#include <stdio.h>
#include <stdlib.h>

#if defined (__is_libk)
#include <kernel/kpanic.h>
#endif

__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
	kpanic("abort() has been called (please use kpanic directly instead of abort)\n");
#else
	// TODO: Abnormally terminate the process as if by SIGABRT.
	printf("abort()\n");
#endif
	while (1) { }
	__builtin_unreachable();
}
