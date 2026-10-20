/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "uart.h"
#include "../../io.h"

void uart_init(uint16_t base, uint32_t baud) {
	uart_set_baud(base, baud);

	outb(UART_REG_FCR(base), UART_FCR_ENABLE | UART_FCR_CLEAR_RX | UART_FCR_CLEAR_TX | UART_FCR_TRIGGER_14);
	outb(UART_REG_MCR(base), UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);

	while (inb(UART_REG_LSR(base)) & UART_LSR_DATA_READY) {
		inb(UART_REG_DATA(base));
	}
}

bool uart_probe(uint16_t base, serial_chip_t* chip) {
	outb(UART_REG_SCRATCH(base), 0xAA);
	if (inb(UART_REG_SCRATCH(base)) != 0xAA) {
		*chip = SERIAL_CHIP_NONE;
		return false;
	}

	outb(UART_REG_SCRATCH(base), 0x55);
	if (inb(UART_REG_SCRATCH(base)) != 0x55) {
		*chip = SERIAL_CHIP_NONE;
		return false;
	}

	outb(UART_REG_FCR(base), UART_FCR_ENABLE | UART_FCR_CLEAR_RX | UART_FCR_CLEAR_TX);
	uint8_t fcr = inb(UART_REG_FCR(base));
	*chip = (fcr & 0xC0) == 0xC0 ? SERIAL_CHIP_16550A : SERIAL_CHIP_8250;
	
	return true;
}

bool uart_loopback_test(uint16_t base) {
	uint8_t saved_mcr = inb(UART_REG_MCR(base));

	outb(UART_REG_MCR(base), UART_MCR_LOOPBACK | UART_MCR_OUT2);

	while (!(inb(UART_REG_LSR(base)) & UART_LSR_TX_EMPTY)) {}
	outb(UART_REG_DATA(base), 0xAE);

	for (int i = 0; i < 100000; i++) {
		if (inb(UART_REG_LSR(base)) & UART_LSR_DATA_READY) {
			break;
		}
	}

	bool ok = (inb(UART_REG_LSR(base)) & UART_LSR_DATA_READY) && inb(UART_REG_DATA(base)) == 0xAE;

	outb(UART_REG_MCR(base), saved_mcr);

	while (inb(UART_REG_LSR(base)) & UART_LSR_DATA_READY) {
		inb(UART_REG_DATA(base));
	}

	return ok;
}

void uart_set_baud(uint16_t base, uint32_t baud) {
	uint16_t divisor = (uint16_t)(UART_BASE_CLOCK / baud);

	outb(UART_REG_LCR(base), UART_LCR_DLAB | UART_LCR_8N1);
	outb(UART_REG_DATA(base), (uint8_t)(divisor & 0xFF));
	outb(UART_REG_IER(base), (uint8_t)((divisor >> 8) & 0xFF));
	outb(UART_REG_LCR(base), UART_LCR_8N1);
}

bool uart_tx_ready(uint16_t base) {
	return (inb(UART_REG_LSR(base)) & UART_LSR_TX_EMPTY) != 0;
}

void uart_putchar(uint16_t base, char c) {
	while (!uart_tx_ready(base)) {}

	if (c == '\n') {
		outb(UART_REG_DATA(base), '\r');
		while (!uart_tx_ready(base)) {}
	}

	outb(UART_REG_DATA(base), (uint8_t)c);
}

void uart_putchar_raw(uint16_t base, char c) {
	outb(UART_REG_DATA(base), (uint8_t)c);
}

bool uart_rx_ready(uint16_t base) {
	return (inb(UART_REG_LSR(base)) & UART_LSR_DATA_READY) != 0;
}

uint8_t uart_getchar(uint16_t base) {
	return inb(UART_REG_DATA(base));
}

uint8_t uart_read_lsr(uint16_t base) {
	return inb(UART_REG_LSR(base));
}

void uart_enable_rx_irq(uint16_t base) {
	outb(UART_REG_IER(base), UART_IER_RX_AVAIL);
}

void uart_enable_tx_irq(uint16_t base) {
	outb(UART_REG_IER(base), UART_IER_RX_AVAIL | UART_IER_TX_EMPTY);
}

void uart_disable_tx_irq(uint16_t base) {
	outb(UART_REG_IER(base), UART_IER_RX_AVAIL);
}
