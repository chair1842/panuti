#ifndef _KERNEL_MEMMAN_TEMP_MAP_H
#define _KERNEL_MEMMAN_TEMP_MAP_H
#include <stdint.h>
#include <stddef.h>

// shoo shoo, dont see my vibecoded code.

/*
 * Temporarily maps `size` bytes of physical memory starting at `phys`
 * into kernel virtual space, at a fixed scratch address. Returns a
 * kernel-usable pointer to the start of that mapping.
 *
 * NOTE: not reentrant / not thread-safe. Only one temp mapping may be
 * active at a time. Always pair with unmap_physical_temp before starting
 * another. Fine for now since the kernel is single-threaded during boot
 * and task setup; revisit if that ever changes.
 */
void* map_physical_temp(uint32_t phys, size_t size);

// Unmaps a region previously returned by map_physical_temp.
void unmap_physical_temp(void* virt, size_t size);

#endif