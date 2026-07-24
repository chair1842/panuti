#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/tty.h>
#include "vga.h"

static int vga_write(void* impl, const void* buf, size_t len) {
	(void)impl;
	const char* p = (const char*)buf;
	for (size_t i = 0; i < len; i++) {
		terminal_putchar(p[i]);
	}
	return (int)len;
}

static const handle_ops_t vga_file_ops = {
	.read = op_not_supported_rw,
	.write = vga_write,
	.activate = op_not_supported_act,
	.wait = op_not_supported_wait,
	.close = op_not_supported_close,
};

void vga_register_console(void) {
	registry_add("/dvc/console", INODE_FILE, NULL, &vga_file_ops);
}