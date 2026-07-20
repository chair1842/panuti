#include <kernel/sched/task.h>
#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/tempmap.h>

#include <kernel/elf.h>
#include "../arch/i386/memman/pmm/pmm.h"

extern void (*test_payload_install(addr_space_t addr_space))(void);

void kernel_main(void) {
	terminal_initialize();
	printf("Panuti Kernel\n");

	{
		uint32_t module_size = init_module_phys_end - init_module_phys_start;
		const void* elf_data = map_physical_temp(init_module_phys_start, module_size);

		elf_loadable_segment_t segs[16];
		int nsegs;
		uint64_t entry;

		elf_result_t r = elf32_parse(elf_data, module_size, segs, 16, &nsegs, &entry);
		klog(KLOG_INFO, "elf32_parse result = %d\n", r);

		if (r == ELF_OK) {
			klog(KLOG_INFO, "entry = 0x%x, nsegs = %d\n", (uint32_t)entry, nsegs);

			// load into the CURRENT address space (no task yet) just to
			// prove elf_load_segments' copy/zero-fill logic works
			int load_result = elf_load_segments(memman_get_kernel_addr_space(), elf_data, segs, nsegs);
			klog(KLOG_INFO, "elf_load_segments result = %d\n", load_result);

			if (load_result == 0) {
				// segment 0's vaddr should now contain real code -- read
				// the first few bytes back and print them to confirm the
				// copy actually landed where it should
				uint8_t* loaded = (uint8_t*)(uint32_t)segs[0].vaddr;
				klog(KLOG_INFO, "first bytes at vaddr: %x %x %x %x\n", loaded[0], loaded[1], loaded[2], loaded[3]);
			}
		}

		unmap_physical_temp((void*)elf_data, module_size);
	}
}