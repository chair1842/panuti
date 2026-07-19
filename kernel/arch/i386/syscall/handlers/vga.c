#include "handlers.h"
#include <panuti/errno.h>
#include <stdint.h>
#include <kernel/tty.h>

int32_t syshandler_vga(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	char c = (char)a1;
	terminal_putchar(c);
	return PANUTIERRNO_PLAINSUCCESS;
}