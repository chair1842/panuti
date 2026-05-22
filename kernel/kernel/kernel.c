#include <stdio.h>

#include <kernel/tty.h>

void kernel_main(void) {
	terminal_initialize();
	printf("Panuti Kernel\n");
	volatile int i = 1;
	volatile int j = 0;
	volatile int k = i / j; // trigger a page fault
}
