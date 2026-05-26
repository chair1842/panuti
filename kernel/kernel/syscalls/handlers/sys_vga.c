#include "handlers.h"
#include <kernel/tty.h>

// sys_vga(char* str, uint32_t len)
// temporary
uint32_t sys_vga(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
	terminal_write((const char*)arg1, arg2);
	return 0;
}