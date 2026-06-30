#ifndef _KERNEL_SERIAL_H
#define _KERNEL_SERIAL_H

#include <stddef.h>

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char* data, size_t size);
void serial_writestring(const char* data);

#endif