/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/irq.h>
#include <kernel/memman/memman.h>
#include <panuti/errno.h>
#include <panuti/mmap.h>
#include <stdint.h>

/* Frames committed per pass. A pass holds the interrupt mask while it allocates
 * and maps, so this bounds the irq-off window as well as the stack space used
 * for the physical address list. */
#define MMAPAN_CHUNK 64

int32_t syshandler_mmapan(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	size_t len = a1;
	uint32_t prot = a2;
	uint32_t hint = a3;

	/* An unknown bit means the caller was built against a different header, so
	 * refuse instead of quietly mapping with a protection nobody asked for. */
	if (prot & ~(uint32_t)(MMAPAN_PROT_READ | MMAPAN_PROT_WRITE | MMAPAN_PROT_EXEC)) {
		return PANUTIERRNO_INVALIDADDR;
	}
	if (len == 0 || len > MMAPAN_MAX) {
		return PANUTIERRNO_INVALIDADDR;
	}

	/* len is bounded by MMAPAN_MAX above, so this cannot overflow. */
	uint32_t npages = ((uint32_t)len + 0xFFF) >> 12;

	/* The address is chosen entirely here, so there is no user pointer to
	 * validate; the gap finder already stays inside user space. The hint is
	 * advisory and falls back to a search when it does not pan out. */
	uint32_t base = memman_find_free_run(hint, npages);
	if (base == 0) {
		return PANUTIERRNO_NOMEM;
	}

	/* PROT_NONE stays supervisor-only, which is exactly what makes those pages
	 * unreachable from ring 3. READ or WRITE make them user accessible, and
	 * since the x86 R/W bit gates supervisor access as well, a writable page is
	 * readable too -- PROT_WRITE alone implies read. PROT_EXEC has no
	 * representation in a 32-bit PTE, so it adds nothing. */
	uint32_t flags = MEMMAN_PRESENT;
	if (prot & (MMAPAN_PROT_READ | MMAPAN_PROT_WRITE)) {
		flags |= MEMMAN_USER;
	}
	if (prot & MMAPAN_PROT_WRITE) {
		flags |= MEMMAN_RW;
	}

	uint32_t phys[MMAPAN_CHUNK];
	uint32_t mapped = 0;

	while (mapped < npages) {
		uint32_t chunk = npages - mapped;
		if (chunk > MMAPAN_CHUNK) {
			chunk = MMAPAN_CHUNK;
		}
		uint32_t vbase = base + (mapped << 12);

		/* pmm_allocp() is an unsynchronized read-modify-write against a shared
		 * bitmap and the kernel is preemptive, so a whole allocate-and-map pass
		 * runs with interrupts masked. */
		uint32_t saved_flags = irq_save_disable();

		uint32_t got = 0;
		while (got < chunk) {
			uint32_t frame = memman_alloc_frame();
			if (frame == 0) {
				break; /* 0 doubles as the out-of-memory signal */
			}
			phys[got++] = frame;
		}

		if (got < chunk) {
			for (uint32_t i = 0; i < got; i++) {
				memman_free_frame(phys[i]);
			}
			irq_restore(saved_flags);
			/* Release every page earlier passes already committed. */
			memman_unmap_run_free(base, mapped);
			return PANUTIERRNO_NOMEM;
		}

		/* On failure this unmaps the current chunk itself, so only the frames
		 * it may have installed need releasing here. */
		if (memman_map_zeroed_run(vbase, phys, chunk, flags) != 0) {
			for (uint32_t i = 0; i < chunk; i++) {
				memman_free_frame(phys[i]);
			}
			irq_restore(saved_flags);
			memman_unmap_run_free(base, mapped);
			return PANUTIERRNO_NOMEM;
		}

		mapped += chunk;
		irq_restore(saved_flags);
	}

	return (int32_t)base;
}
