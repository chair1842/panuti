#include <kernel/serial.h>

#include <stdint.h>
#include <stdbool.h>

#include "../../io.h"

// Serial port I/O ports
#define SERIAL_COM1_BASE 0x3F8
#define SERIAL_DATA_PORT(base) (base)
#define SERIAL_FIFO_COMMAND_PORT(base) (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base) (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base) (base + 5)

// Serial port configuration
#define SERIAL_LINE_ENABLE_DLAB 0x80

static int serial_is_transmit_fifo_empty(uint16_t base) {
	return inb(SERIAL_LINE_STATUS_PORT(base)) & 0x20;
}

void serial_init(void) {
	uint16_t base = SERIAL_COM1_BASE;

	outb(SERIAL_LINE_COMMAND_PORT(base), SERIAL_LINE_ENABLE_DLAB);
	outb(SERIAL_DATA_PORT(base), 0x03); // 38400 baud
	outb(SERIAL_DATA_PORT(base) + 1, 0x00);
	outb(SERIAL_LINE_COMMAND_PORT(base), 0x03); // 8 bits, no parity, one stop bit
	outb(SERIAL_FIFO_COMMAND_PORT(base), 0xC7); // Enable FIFO, clear them, with 14-byte threshold
	outb(SERIAL_MODEM_COMMAND_PORT(base), 0x0B); // IRQs enabled, RTS/DSR set
}

void serial_putchar(char c) {
	uint16_t base = SERIAL_COM1_BASE;

	while (!serial_is_transmit_fifo_empty(base));

	if (c == '\n') {
		outb(SERIAL_DATA_PORT(base), '\r');
		while (!serial_is_transmit_fifo_empty(base));
	}

	outb(SERIAL_DATA_PORT(base), c);
}

void serial_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++) {
		serial_putchar(data[i]);
	}
}

void serial_writestring(const char* data) {
	while (*data) {
		serial_putchar(*data++);
	}
}