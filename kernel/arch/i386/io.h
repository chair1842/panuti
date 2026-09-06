#ifndef ARCH_I386_IO_H
#define ARCH_I386_IO_H

#include <stdint.h>
#include <stddef.h>

void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);
void outsw(uint16_t port, const void* buffer, size_t count);
void insw(uint16_t port, void* buffer, size_t count);
void iowait(void);

#endif