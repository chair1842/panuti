/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_SERIAL_UART_H
#define _KERNEL_SERIAL_UART_H

#include <kernel/serial/core.h>
#include <stdbool.h>
#include <stdint.h>

#define UART_COM1_BASE 0x3F8
#define UART_COM2_BASE 0x2F8
#define UART_COM3_BASE 0x3E8
#define UART_COM4_BASE 0x2E8

#define UART_BASE_CLOCK 1843200

void uart_init(uint16_t base, uint32_t baud);
bool uart_probe(uint16_t base, serial_chip_t* chip);
bool uart_loopback_test(uint16_t base);
void uart_set_baud(uint16_t base, uint32_t baud);

bool uart_tx_ready(uint16_t base);
void uart_putchar_raw(uint16_t base, char c);

#endif
