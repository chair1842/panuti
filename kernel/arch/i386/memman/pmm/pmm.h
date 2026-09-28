/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef ARCH_I386_MEMANPMM_H
#define ARCH_I386_MEMANPMM_H

#include <stdint.h>
#include "../../multiboot.h"

void pmm_init(multiboot_info_t* mbi);
uint32_t pmm_allocp(void);
void pmm_freep(uint32_t address);

// frame capacity of the bitmap, sized at boot from the e820 map rather than
// fixed to the 4GiB an i386 can theoretically address.
uint32_t pmm_nframes(void);

// bytes of bitmap backing pmm_nframes() frames
uint32_t pmm_bitmap_bytes(void);

// out-of-range bit operations that were ignored instead of written. these are
// bugs somewhere upstream; they are counted rather than fatal because they turn
// up on error unwind paths. a nonzero count at boot means something is freeing
// or reserving frames the map never described.
uint32_t pmm_rejects(void);

extern uint32_t init_module_phys_start;
extern uint32_t init_module_phys_end;

#endif