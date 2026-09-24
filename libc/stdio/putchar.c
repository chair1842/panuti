#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#include <kernel/serial.h>
#else
#include <panuti/syscall/syscallsf.h>
#endif

int putchar(int ic) {
#if defined(__is_libk)
	char c = (char)ic;
	terminal_write(&c, sizeof(c));
	serial_write(&c, sizeof(c));
#else
	char c = (char)ic;
	panutisysf_stream_write(0, &c, sizeof(c));
#endif
	return ic;
}
