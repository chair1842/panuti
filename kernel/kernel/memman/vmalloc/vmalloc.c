#include <kernel/memman/vmalloc.h>
#include <stdint.h>
#include <kernel/klog.h>
#include <kernel/memman/memman.h>

#define PAGE_ALIGN_UP(addr) (((addr) + 0xFFF) & ~0xFFF)

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2

/*
 * The kernel heap grows upward from _kernel_end. It must never run into the
 * fixed scratch windows used for temporary mappings -- vmm_create_page_dir()
 * maps PAGE_DIR_TEMP_MAP, map_physical_temp() uses TEMP_MAP_BASE, and the
 * ELF loader uses ELF_LOAD_SCRATCH. Those live at 0xC0800000+, so the heap is
 * capped just below them.
 */
#define VMALLOC_END 0xC0800000

extern uint32_t _kernel_end;
static uint32_t vmalloc_next;

static uint32_t irq_save_disable(void) {
	uint32_t flags;
	__asm__ __volatile__("pushf\n\tpop %0\n\tcli" : "=r"(flags) :: "memory");
	return flags;
}

static void irq_restore(uint32_t flags) {
	__asm__ __volatile__("push %0\n\tpopf" :: "r"(flags) : "memory", "cc");
}

void vmalloc_init(void) {
	vmalloc_next = PAGE_ALIGN_UP((uint32_t)&_kernel_end);
	if (vmalloc_next == 0) {
		vmalloc_next = 4096;
	}
}

void* vmalloc_pg(void) {
	uint32_t flags = irq_save_disable();

	if (vmalloc_next > VMALLOC_END - 4096) {
		klog(KLOG_WARN, "vmalloc: heap exhausted at %p\n", (void*)vmalloc_next);
		irq_restore(flags);
		return 0;
	}

	uint32_t phys = memman_alloc_frame();
	if (phys == 0) {
		irq_restore(flags);
		return 0;
	}

	uint32_t virt = vmalloc_next;
	vmalloc_next += 4096;

	memman_map(virt, phys, PAGE_PRESENT | PAGE_RW);

	irq_restore(flags);
	return (void*)virt;
}

// Maps `npages` consecutive virtual pages (fresh physical frames each) in one
// atomic step so concurrent callers can never interleave and fragment the range.
void* vmalloc_pages(uint32_t npages) {
	if (npages == 0 || npages > 65536) {
		return NULL;
	}

	uint32_t flags = irq_save_disable();
	uint32_t base = vmalloc_next;

	if (base > VMALLOC_END || npages > (VMALLOC_END - base) / 4096) {
		klog(KLOG_WARN, "vmalloc: heap exhausted for %u pages\n", npages);
		irq_restore(flags);
		return NULL;
	}

	for (uint32_t i = 0; i < npages; i++) {
		uint32_t phys = memman_alloc_frame();
		if (phys == 0) {
			while (i > 0) {
				i--;
				vmalloc_free((void*)(base + i * 4096));
			}
			irq_restore(flags);
			return NULL;
		}
		memman_map(base + i * 4096, phys, PAGE_PRESENT | PAGE_RW);
	}

	vmalloc_next += npages * 4096;
	irq_restore(flags);

	return (void*)base;
}

void vmalloc_free_pages(void* addr, uint32_t npages) {
	for (uint32_t i = 0; i < npages; i++) {
		vmalloc_free((void*)((uint32_t)addr + i * 4096));
	}
}

void vmalloc_free(void* addr) {
	uint32_t virt = (uint32_t)addr;
	uint32_t phys = memman_get_phys(virt);
	if (phys == 0) {
		return;
	}

	memman_unmap(virt);
	memman_free_frame(phys);
}