#include <stdarg.h>
#include "printf.h"

int printf(const char* restrict fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int ret = vprintf_(fmt, args);
	va_end(args);
	return ret;
}