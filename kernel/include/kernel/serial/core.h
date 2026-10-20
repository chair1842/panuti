/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_SERIAL_CORE_H
#define _KERNEL_SERIAL_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SERIAL_RING_SIZE 1024
#define SERIAL_MAX_PORTS 2
#define SERIAL_MAX_WAITERS 8

typedef enum serial_chip {
	SERIAL_CHIP_NONE = 0,
	SERIAL_CHIP_8250,
	SERIAL_CHIP_16550A,
} serial_chip_t;

typedef struct {
	uint16_t base;
	uint32_t irq_vector;
	int index;
	serial_chip_t chip;
	bool present;

	uint8_t rx_ring[SERIAL_RING_SIZE];
	volatile size_t rx_head;
	volatile size_t rx_tail;

	uint8_t tx_ring[SERIAL_RING_SIZE];
	volatile size_t tx_head;
	volatile size_t tx_tail;

	void* rx_waiters[SERIAL_MAX_WAITERS];
	int rx_n_waiters;
	void* tx_waiters[SERIAL_MAX_WAITERS];
	int tx_n_waiters;

	volatile uint32_t rx_bytes;
	volatile uint32_t tx_bytes;
	volatile uint32_t overruns;
	uint32_t baud;
} uart_t;

void serial_register(void);

void serial_rx_push(uart_t* u, uint8_t byte);
int serial_tx_pull(uart_t* u, uint8_t* out);
void serial_rx_error(uart_t* u);

uart_t* serial_get_port(int index);
int serial_port_count(void);

void serial_isa_attach(uart_t* u);

#endif
