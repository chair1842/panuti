#include "msr.h"

void rdmsr(uint32_t msr, uint32_t* lo, uint32_t* hi) {
	__asm__ __volatile__("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi) {
	__asm__ __volatile__("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

void wrmsr32(uint32_t msr, uint32_t value) {
	wrmsr(msr, value, 0);
}