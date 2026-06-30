#include <stdio.h>
#include "printf.h"

int vprintf(const char* format, va_list arg) {
	return vprintf_(format, arg);
}