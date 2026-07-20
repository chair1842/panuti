#include <kernel/sched/task.h>
#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/memman/memman.h>

#include <kernel/elf.h>
#include "../arch/i386/memman/pmm/pmm.h"

extern void (*test_payload_install(addr_space_t addr_space))(void);

#define MODULE_TEMP_MAP_VIRT 0xC0600000
#define PMM_PAGE_SIZE 4096

void kernel_main(void) {
	terminal_initialize();
	printf("Panuti Kernel\n");

	{
		uint32_t module_phys_start = init_module_phys_start;
		uint32_t module_size = init_module_phys_end - init_module_phys_start;
		uint32_t num_pages = (module_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

		// map the module's physical pages into kernel virtual space temporarily,
		// since the boot-time identity map is torn down by vmm_init() before this runs.
		for (uint32_t i = 0; i < num_pages; i++) {
			memman_map(MODULE_TEMP_MAP_VIRT + i * PMM_PAGE_SIZE,
			           module_phys_start + i * PMM_PAGE_SIZE,
			           MEMMAN_PRESENT | MEMMAN_RW);
		}

		elf_loadable_segment_t segs[16];
		int nsegs;
		uint64_t entry;

		const void* elf_data = (const void*)MODULE_TEMP_MAP_VIRT;
		elf_result_t r = elf32_parse(elf_data, module_size, segs, 16, &nsegs, &entry);

		klog(KLOG_INFO, "elf32_parse result = %d\n", r);
		if (r == ELF_OK) {
			klog(KLOG_INFO, "entry = 0x%x, nsegs = %d\n", (uint32_t)entry, nsegs);

			for (int i = 0; i < nsegs; i++) {
				klog(KLOG_INFO, "  seg[%d]: vaddr=0x%x offset=0x%x filesz=0x%x memsz=0x%x flags=0x%x\n",
					i,
					(uint32_t)segs[i].vaddr,
					(uint32_t)segs[i].offset,
					(uint32_t)segs[i].filesz,
					(uint32_t)segs[i].memsz,
					segs[i].flags);
			}
		}

		// unmap now that parsing is done
		for (uint32_t i = 0; i < num_pages; i++) {
			memman_unmap(MODULE_TEMP_MAP_VIRT + i * PMM_PAGE_SIZE);
		}
	}
}