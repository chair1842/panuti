/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "uart.h"
#include "../../intpt/handlers/main.h"
#include <kernel/klog.h>
#include <kernel/serial/core.h>

#define SERIAL_VECTOR_COM1 0x24
#define SERIAL_VECTOR_COM2 0x23

static uart_t* com1_port;
static uart_t* com2_port;

static void serial_drain(uart_t* u) {
	while (1) {
		uint8_t lsr = uart_read_lsr(u->base);

		if (lsr & UART_LSR_OVERRUN) {
			u->overruns++;
		}

		if (!(lsr & UART_LSR_DATA_READY)) {
			break;
		}

		serial_rx_push(u, uart_getchar(u->base));
	}
}

static void serial_com1_isr(registers_t* regs) {
	(void)regs;
	if (com1_port) {
		serial_drain(com1_port);
	}
}

static void serial_com2_isr(registers_t* regs) {
	(void)regs;
	if (com2_port) {
		serial_drain(com2_port);
	}
}

void serial_isa_attach(uart_t* u) {
	if (u->base == UART_COM1_BASE) {
		com1_port = u;
		uart_enable_rx_irq(u->base);
		register_handler(SERIAL_VECTOR_COM1, serial_com1_isr);
		klog(KLOG_INFO, "serial: COM1 rx irq registered at vector 0x%x\n", SERIAL_VECTOR_COM1);
	} else if (u->base == UART_COM2_BASE) {
		com2_port = u;
		uart_enable_rx_irq(u->base);
		register_handler(SERIAL_VECTOR_COM2, serial_com2_isr);
		klog(KLOG_INFO, "serial: COM2 rx irq registered at vector 0x%x\n", SERIAL_VECTOR_COM2);
	}
}
