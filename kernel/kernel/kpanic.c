#include <kernel/kpanic.h>
#include <kernel/tty.h>
#include <kernel/serial.h>
#include <stdarg.h>
#include <stdio.h>

#define FG_COLOR_PANIC ANSI_COLOR_LIGHT_RED
#define BG_COLOR_PANIC ANSI_COLOR_BROWN

void kpanic_impl(const char *file, int line, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	terminal_clear();
	terminal_fsetcolor(FG_COLOR_PANIC, BG_COLOR_PANIC); 

	printf("[PANIC] SYSTEM HALTED\n");
	printf("[PANIC] RESTART YOUR SYSTEM\n");
	printf("[PANIC] %s:%d: ", file, line);
	vprintf(fmt, args);
	va_end(args);

	asm volatile ("cli");
	for (;;) { asm volatile ("hlt"); }
}