#include <kernel/boot_mod.h>
#include <kernel/memman/tempmap.h>
#include "memman/pmm/pmm.h"

int kernel_get_init_module(const void** out_data, size_t* out_size) {
	if (init_module_phys_start == 0 || init_module_phys_end <= init_module_phys_start) {
		return -1;
	}

	uint32_t size = init_module_phys_end - init_module_phys_start;
	const void* mapped = map_physical_temp(init_module_phys_start, size);

	*out_data = mapped;
	*out_size = size;
	return 0;
}