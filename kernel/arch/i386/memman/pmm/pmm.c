/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "pmm.h"
#include <kernel/kpanic.h>
#include <kernel/klog.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define PMM_PAGE_SIZE 4096
#define PMM_PAGE_MASK (PMM_PAGE_SIZE - 1)

#define LOWMEM_START 0x0
#define LOWMEM_END 0x100000

#define KERNEL_START 0x100000
#ifndef KERNEL_END
extern uint32_t _kernel_end;
#define KERNEL_END ((uint32_t)&_kernel_end - 0xC0000000)
#endif

// the highest physical address a uint32_t can carry. e820 regions reaching past
// it are clamped down; a frame index is what we store, and 0xFFFFFFFF/4096 is
// the last one a 32-bit address can name.
#define PMM_PHYS_LIMIT 0x100000000ULL

// paging is already on by the time pmm_init runs (boot.S enables it well before
// the call), and the boot page table identity-maps the first 4MiB. That is what
// lets the carve be reached while pmm_init stands it up, so the carve is
// confined to the first 4MiB and a higher one is refused rather than aliased.
#define PMM_IDENTITY_LIMIT 0x400000

// The bitmap's permanent virtual address.
//
// It cannot keep the identity address, because the kernel heap starts at
// PAGE_ALIGN_UP(&_kernel_end) and grows to 0xC0800000, and under the identity
// map virtual 0xC015c000 *is* physical 0x15c000 -- the first page above the
// kernel image, which is exactly where a carve placed right after the image
// lands. The first slab would map a different frame over that address and the
// allocator would silently start reading and writing slab data, which is what
// putting the bitmap in low memory actually gets you.
//
// 0xC0C00000 is clear of everything: the heap stops at 0xC0800000 and the
// scratch windows live at 0xC0800000, 0xC0820000 and 0xC0A00000, all inside
// PDE 0x302. This is PDE 0x303, and vmm_create_page_dir() copies the kernel
// PDEs into every task directory, so tasks inherit the mapping.
#define PMM_BITMAP_VIRT 0xC0C00000u
#define PMM_BITMAP_PDE (PMM_BITMAP_VIRT >> 22)
#define RECURSIVE_TABLE_BASE 0xFFC00000u

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2

// where the carved bitmap lives during pmm_init, before it has a home of its
// own. the test harness compiles this file for the host, where the identity
// window means nothing, and substitutes a canary'd buffer -- an out-of-range bit
// poke then shows up as a smashed canary instead of a segfault.
#ifdef PMM_TEST_BITMAP
extern uint8_t* pmm_test_bitmap_base;
#define PMM_BITMAP_BOOT(phys) ((uint8_t*)pmm_test_bitmap_base)
#define PMM_BITMAP_HOME ((uint8_t*)pmm_test_bitmap_base)
#else
#define PMM_BITMAP_BOOT(phys) ((uint8_t*)((phys) + 0xC0000000u))
#define PMM_BITMAP_HOME ((uint8_t*)PMM_BITMAP_VIRT)

extern uint32_t boot_page_dir[1024];

// move the bitmap onto its own page table, so nothing else can remap it.
// pmm_allocp is usable by now: the reservations are all in place, and the
// bitmap is still being read through the identity address.
static void map_bitmap_home(uint32_t phys) {
	uint32_t table_phys = pmm_allocp();
	if (table_phys == 0) {
		kpanic("PMM: no frame for the bitmap page table");
		return;
	}

	boot_page_dir[PMM_BITMAP_PDE] = table_phys | PAGE_PRESENT | PAGE_RW;

	// the recursive entry makes any page table reachable, so this does not care
	// where in physical memory the frame landed
	uint32_t* table = (uint32_t*)(RECURSIVE_TABLE_BASE + PMM_BITMAP_PDE * 0x1000);
	memset(table, 0, 0x1000);
	table[0] = phys | PAGE_PRESENT | PAGE_RW;

	__asm__ __volatile__("invlpg (%0)" ::"r"(PMM_BITMAP_VIRT) : "memory");
}
#endif


// out-of-range bit pokes are a bug somewhere upstream, but they arrive on error
// unwind paths where panicking turns a recoverable partial failure into a dead
// kernel, so they are counted and logged a few times rather than fatal. a
// systematically wrong frame in a range free would otherwise emit one serial
// line per page with interrupts off, which is its own way to hang.
#define PMM_WARN_LIMIT 4

// tracks whether each physical page is free or not. each bit covers one 4KiB
// page, 0 means used and 1 means free. it is sized from the e820 map at boot
// rather than fixed, so it describes the memory the machine actually has
// instead of the 4GiB i386 can theoretically address. this used to be a 128KiB
// .bss array covering 2^20 frames, which is only safe because 2^20 is exactly
// the number of frames a uint32_t address can name -- sizing it to real memory
// removes that coincidence, hence the range checks on every write below.
static uint8_t* bitmap;
static uint32_t bitmap_frames;   // capacity, in frames
static uint32_t next_free_page;  // allocation cursor, may wrap
static uint32_t rejected_ops;    // ignored out-of-range calls, for diagnosis

uint32_t init_module_phys_start = 0;
uint32_t init_module_phys_end = 0;

// true when the frame is inside the bitmap we actually have. every bit poke goes
// through this, because a bad index is now an out-of-bounds write rather than a
// harmless read of a neighbouring bit.
static inline bool page_in_range(uint32_t page) {
	return page < bitmap_frames;
}

static void pmm_reject(const char* op, uint32_t page) {
	if (rejected_ops < PMM_WARN_LIMIT) {
		klog(KLOG_WARN, "pmm: %s ignored out-of-range frame %u (capacity %u)\n",
			op, page, bitmap_frames);
	} else if (rejected_ops == PMM_WARN_LIMIT) {
		klog(KLOG_WARN, "pmm: further out-of-range %s calls suppressed\n", op);
	}
	rejected_ops++;
}

// set the bit for a page in the bitmap to 1, marking it free.
void pmm_clrp(uint32_t page) {
	if (!page_in_range(page)) {
		pmm_reject("pmm_clrp", page);
		return;
	}
	bitmap[page / 8] |= (1 << (page % 8));
}

// set the bit for a page in the bitmap to 0, marking it used.
void pmm_setp(uint32_t page) {
	if (!page_in_range(page)) {
		pmm_reject("pmm_setp", page);
		return;
	}
	bitmap[page / 8] &= ~(1 << (page % 8));
}

uint32_t pmm_nframes(void) {
	return bitmap_frames;
}

uint32_t pmm_bitmap_bytes(void) {
	return (bitmap_frames + 7) / 8;
}

uint32_t pmm_rejects(void) {
	return rejected_ops;
}

// every e820 entry, in the order the bootloader laid them out. mmap_length is
// the bound to trust here: the spec's entry->size == 0 terminator is a
// convention, and walking to a zero size is what a malformed map turns into an
// endless walk.
#define MBI_WALK(mbi, entry)                                                                   \
	for (multiboot_memory_map_t* entry = (multiboot_memory_map_t*)((mbi)->mmap_addr);          \
		(uint32_t)(entry) < (mbi)->mmap_addr + (mbi)->mmap_length;                            \
		entry = (multiboot_memory_map_t*)((uint32_t)entry + entry->size + sizeof(uint32_t)))

// clamps an e820 region to what a 32-bit frame index can name
static inline uint64_t mmap_clamped_end(multiboot_memory_map_t* entry) {
	uint64_t end = entry->addr + entry->len;
	return end > PMM_PHYS_LIMIT ? PMM_PHYS_LIMIT : end;
}

// push a candidate start address past an already-reserved range if it lands
// inside one, so a single region that straddles low memory or the kernel image
// still yields a usable slot above it.
static inline uint32_t carve_skip(uint32_t start, uint32_t limit, uint32_t skip_start, uint32_t skip_end) {
	if (start >= skip_start && start < skip_end && skip_end <= limit) {
		uint32_t past = (skip_end + PMM_PAGE_MASK) & ~PMM_PAGE_MASK;
		return past <= limit ? past : limit;
	}
	return start;
}

// find room for the bitmap itself. the bitmap cannot come from the allocator it
// bootstraps -- kmalloc goes slab -> vmalloc_pg -> memman_alloc_frame ->
// pmm_allocp, which is this file -- so it is carved straight out of the e820
// map, skipping everything already spoken for. returns false if nothing fits.
static bool carve_place(uint32_t nbytes, multiboot_info_t* mbi, uint32_t mod_start, uint32_t mod_end, uint32_t* out_phys) {
	if (nbytes == 0) {
		return false;
	}

	MBI_WALK(mbi, entry) {
		if (entry->type != MULTIBOOT_MEMORY_AVAILABLE || entry->addr >= PMM_PHYS_LIMIT) {
			continue;
		}

		uint64_t end = mmap_clamped_end(entry);
		if (end <= LOWMEM_END) {
			continue;
		}

		// the carve has to be addressable, so it cannot sit above the identity
		// window. adding a pte to boot_page_table would lift this, but nothing
		// has ever needed it and a clear panic beats a wild pointer.
		uint32_t limit = (uint32_t)end;
		if (limit > PMM_IDENTITY_LIMIT) {
			limit = PMM_IDENTITY_LIMIT;
		}

		uint32_t start = LOWMEM_END;
		start = carve_skip(start, limit, KERNEL_START, KERNEL_END);
		start = carve_skip(start, limit, mod_start, mod_end);
		start = (start + PMM_PAGE_MASK) & ~PMM_PAGE_MASK;

		if (limit > start && (uint64_t)(limit - start) >= nbytes) {
			*out_phys = start;
			return true;
		}
	}

	return false;
}

void pmm_init(multiboot_info_t* mbi) {
	rejected_ops = 0;

	uint8_t mbi_imp = (mbi->flags >> 6) & 1;
	if (!mbi_imp) {
		// No memory map provided by the bootloader.
		// Panic and curse the bootloader with the souls of a million devils.
		kpanic("PMM: Memory map not provided by bootloader");
		return;
	}

	uint32_t mod_start = 0;
	uint32_t mod_end = 0;
	uint8_t mods_imp = (mbi->flags >> 3) & 1;
	if (mods_imp && mbi->mods_count > 0) {
		multiboot_module_t* mods = (multiboot_module_t*)mbi->mods_addr;
		mod_start = mods[0].mod_start;
		mod_end = mods[0].mod_end;
	}

	// pass one: how much memory are we being told about? the bitmap has to be
	// sized before it can be used, and marking free regions needs it, so the
	// sizing cannot be folded into the marking the way it used to be.
	uint64_t max_end = 0;
	MBI_WALK(mbi, entry) {
		if (entry->type != MULTIBOOT_MEMORY_AVAILABLE || entry->addr >= PMM_PHYS_LIMIT) {
			continue;
		}
		uint64_t end = mmap_clamped_end(entry);
		if (end > max_end) {
			max_end = end;
		}
	}

	if (max_end == 0) {
		kpanic("PMM: bootloader reported no available memory");
		return;
	}

	bitmap_frames = (uint32_t)((max_end + PMM_PAGE_MASK) / PMM_PAGE_SIZE);
	uint32_t bitmap_bytes = pmm_bitmap_bytes();

	uint32_t bitmap_phys = 0;
	if (!carve_place(bitmap_bytes, mbi, mod_start, mod_end, &bitmap_phys)) {
		kpanic("PMM: nowhere to place the frame bitmap");
		return;
	}

	bitmap = PMM_BITMAP_BOOT(bitmap_phys);
	// zero means used, so clearing it is the safe starting state even if the
	// frames we just took were not fresh.
	memset(bitmap, 0, bitmap_bytes);

	// pass two: mark every available region free. the end rounds up so a region
	// whose length is not a whole number of pages still gets its tail page --
	// truncating here used to leave that page neither free nor reserved.
	MBI_WALK(mbi, entry) {
		if (entry->type != MULTIBOOT_MEMORY_AVAILABLE || entry->addr >= PMM_PHYS_LIMIT) {
			continue;
		}

		uint64_t end = mmap_clamped_end(entry);
		uint32_t start_page = (uint32_t)(entry->addr / PMM_PAGE_SIZE);
		uint32_t end_page = (uint32_t)((end + PMM_PAGE_MASK) / PMM_PAGE_SIZE);

		for (uint32_t page = start_page; page < end_page; page++) {
			pmm_clrp(page);
		}
	}

	// now we mark low memory and kernel memory as used, since we know we will be using it.
	for (uint32_t page = LOWMEM_START / PMM_PAGE_SIZE; page < LOWMEM_END / PMM_PAGE_SIZE; page++) {
		pmm_setp(page);
	}
	for (uint32_t page = KERNEL_START / PMM_PAGE_SIZE; page < (KERNEL_END + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE; page++) {
		pmm_setp(page);
	}

	// reserve any multiboot modules (e.g. init.elf) so pmm_allocp() never hands out their frames.
	if (mod_end > mod_start) {
		multiboot_module_t* mods = (multiboot_module_t*)mbi->mods_addr;
		for (uint32_t i = 0; i < mbi->mods_count; i++) {
			uint32_t start_page = mods[i].mod_start / PMM_PAGE_SIZE;
			uint32_t end_page = (mods[i].mod_end + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
			for (uint32_t page = start_page; page < end_page; page++) {
				pmm_setp(page);
			}
		}

		// stash the first module's location for the ELF loader to use later.
		init_module_phys_start = mod_start;
		init_module_phys_end = mod_end;
	}

	// last, because everything above marks a whole region free and would
	// otherwise hand the bitmap's own frames back out
	uint32_t self_start = bitmap_phys / PMM_PAGE_SIZE;
	uint32_t self_end = (bitmap_phys + bitmap_bytes + PMM_PAGE_MASK) / PMM_PAGE_SIZE;
	for (uint32_t page = self_start; page < self_end; page++) {
		pmm_setp(page);
	}

	next_free_page = 0;

#ifndef PMM_TEST_BITMAP
	// now that everything is reserved, give the bitmap an address the kernel
	// heap can never remap out from under it, and move onto it
	map_bitmap_home(bitmap_phys);
	bitmap = PMM_BITMAP_HOME;
#endif

	// one line at boot, so a machine with a different amount of ram shows its
	// own sizing here instead of having to be re-derived. a nonzero reject
	// count in the same breath means something is poking frames this map never
	// described.
	klog(KLOG_INFO, "pmm: %u frames, %u bytes of bitmap at 0x%x, %u rejects\n",
		bitmap_frames, bitmap_bytes, bitmap_phys, rejected_ops);
}

// walks the cursor forward, wrapping once so a page freed below the cursor is
// still found rather than becoming unallocatable for the rest of the boot.
uint32_t pmm_allocp(void) {
	for (uint32_t pass = 0; pass < 2; pass++) {
		for (uint32_t page = next_free_page; page < bitmap_frames; page++) {
			if (bitmap[page / 8] & (1 << (page % 8))) {
				next_free_page = page + 1;
				pmm_setp(page);
				return page * PMM_PAGE_SIZE;
			}
		}
		next_free_page = 0;
	}

	// no free pages found. return 0 to indicate failure. frame 0 is reserved at
	// init and pmm_freep refuses it, so 0 is never a real answer.
	return 0;
}

void pmm_freep(uint32_t address) {
	// frame 0 doubles as the out-of-memory signal from pmm_allocp, so letting it
	// become free would make a successful allocation look like a failure. the
	// caller in vmm_unmap_run_free has been guarding this by hand; refuse it
	// here instead, at the one place that matters.
	if (address == 0) {
		return;
	}

	// mark the page at the given address as free.
	uint32_t page = address / PMM_PAGE_SIZE;
	if (!page_in_range(page)) {
		pmm_reject("pmm_freep", page);
		return;
	}

	// the cursor may have walked past this frame already
	if (page < next_free_page) {
		next_free_page = page;
	}

	pmm_clrp(page);
}
