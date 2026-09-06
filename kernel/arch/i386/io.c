#include "io.h"

void outb(uint16_t port, uint8_t value) {
	__asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
	uint8_t ret;
	__asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

void outw(uint16_t port, uint16_t value) {
	__asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

uint16_t inw(uint16_t port) {
	uint16_t ret;
	__asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

void outsw(uint16_t port, const void* buffer, size_t count) {
	__asm__ volatile ("rep outsw" : "+S"(buffer), "+c"(count) : "d"(port) : "memory");
}

void insw(uint16_t port, void* buffer, size_t count) {
	__asm__ volatile ("rep insw" : "+D"(buffer), "+c"(count) : "d"(port) : "memory");
}

void iowait(void) {
	__asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}