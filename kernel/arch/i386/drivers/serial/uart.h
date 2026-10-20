/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef ARCH_I386_DRIVERS_SERIAL_UART_H
#define ARCH_I386_DRIVERS_SERIAL_UART_H

#include <kernel/serial/core.h>
#include <stdbool.h>
#include <stdint.h>

#define UART_COM1_BASE 0x3F8
#define UART_COM2_BASE 0x2F8

#define UART_IRQ_COM1 4
#define UART_IRQ_COM2 3

#define UART_REG_DATA(base) ((base) + 0)
#define UART_REG_IER(base) ((base) + 1)
#define UART_REG_FCR(base) ((base) + 2)
#define UART_REG_LCR(base) ((base) + 3)
#define UART_REG_MCR(base) ((base) + 4)
#define UART_REG_LSR(base) ((base) + 5)
#define UART_REG_MSR(base) ((base) + 6)
#define UART_REG_SCRATCH(base) ((base) + 7)

#define UART_LSR_DATA_READY 0x01
#define UART_LSR_OVERRUN 0x02
#define UART_LSR_TX_EMPTY 0x20
#define UART_LSR_TX_IDLE 0x40

#define UART_LCR_DLAB 0x80
#define UART_LCR_8N1 0x03

#define UART_MCR_DTR 0x01
#define UART_MCR_RTS 0x02
#define UART_MCR_OUT2 0x08
#define UART_MCR_LOOPBACK 0x10

#define UART_FCR_ENABLE 0x01
#define UART_FCR_CLEAR_RX 0x02
#define UART_FCR_CLEAR_TX 0x04
#define UART_FCR_TRIGGER_14 0xC0

#define UART_IER_RX_AVAIL 0x01
#define UART_IER_TX_EMPTY 0x02

#define UART_BASE_CLOCK 1843200

void uart_init(uint16_t base, uint32_t baud);
bool uart_probe(uint16_t base, serial_chip_t* chip);
bool uart_loopback_test(uint16_t base);
void uart_set_baud(uint16_t base, uint32_t baud);

bool uart_tx_ready(uint16_t base);
void uart_putchar(uint16_t base, char c);
void uart_putchar_raw(uint16_t base, char c);
bool uart_rx_ready(uint16_t base);
uint8_t uart_getchar(uint16_t base);
uint8_t uart_read_lsr(uint16_t base);

void uart_enable_rx_irq(uint16_t base);
void uart_enable_tx_irq(uint16_t base);
void uart_disable_tx_irq(uint16_t base);

#endif
