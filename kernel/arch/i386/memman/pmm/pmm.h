#ifndef ARCH_I386_MEMANPMM_H
#define ARCH_I386_MEMANPMM_H

#include <stdint.h>
#include "../../multiboot.h"

void pmm_init(multiboot_info_t* mbi);
uint32_t pmm_allocp(void);
void pmm_freep(uint32_t address);

extern uint32_t init_module_phys_start;
extern uint32_t init_module_phys_end;

#endif