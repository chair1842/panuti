#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/kpanic.h>

void kernel_main(void) {
	terminal_initialize();
	printf("Panuti Kernel\n");
}
