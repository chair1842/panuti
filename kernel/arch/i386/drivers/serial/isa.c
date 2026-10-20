/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "uart.h"
#include "../../intpt/handlers/main.h"
#include <kernel/klog.h>
#include <kernel/serial/core.h>

#define SERIAL_VECTOR_IRQ4 0x24
#define SERIAL_VECTOR_IRQ3 0x23

#define SERIAL_MAX_SHARED_PER_IRQ (SERIAL_MAX_PORTS / 2)

static uart_t* irq4_ports[SERIAL_MAX_SHARED_PER_IRQ];
static int irq4_count;
static bool irq4_registered;

static uart_t* irq3_ports[SERIAL_MAX_SHARED_PER_IRQ];
static int irq3_count;
static bool irq3_registered;

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

static void serial_irq4_isr(registers_t* regs) {
	(void)regs;
	for (int i = 0; i < irq4_count; i++) {
		serial_drain(irq4_ports[i]);
	}
}

static void serial_irq3_isr(registers_t* regs) {
	(void)regs;
	for (int i = 0; i < irq3_count; i++) {
		serial_drain(irq3_ports[i]);
	}
}

void serial_isa_attach(uart_t* u) {
	uint32_t vector;
	int irq;
	uart_t** ports;
	int* count;
	bool* registered;

	if (u->base == UART_COM1_BASE || u->base == UART_COM3_BASE) {
		vector = SERIAL_VECTOR_IRQ4;
		irq = UART_IRQ_COM1;
		ports = irq4_ports;
		count = &irq4_count;
		registered = &irq4_registered;
	} else if (u->base == UART_COM2_BASE || u->base == UART_COM4_BASE) {
		vector = SERIAL_VECTOR_IRQ3;
		irq = UART_IRQ_COM2;
		ports = irq3_ports;
		count = &irq3_count;
		registered = &irq3_registered;
	} else {
		return;
	}

	if (*count >= SERIAL_MAX_SHARED_PER_IRQ) {
		klog(KLOG_WARN, "serial: too many ports on IRQ%d\n", irq);
		return;
	}

	ports[(*count)++] = u;

	uart_enable_rx_irq(u->base);

	if (!*registered) {
		if (irq == UART_IRQ_COM1) {
			register_handler(vector, serial_irq4_isr);
		} else {
			register_handler(vector, serial_irq3_isr);
		}
		*registered = true;
		klog(KLOG_INFO, "serial: IRQ%d rx handler registered at vector 0x%x\n", irq, vector);
	}
}
