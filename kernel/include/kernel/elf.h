/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_ELF_H
#define _KERNEL_ELF_H
#include "kernel/memman/memman.h"
#include <stdint.h>
#include <stddef.h>

#define ELF_NIDENT 16

// p_flags bits from the ELF spec. only PF_W is enforceable on this target:
// 32-bit x86 PTEs have no execute bit outside PAE, so PF_X has nothing to
// map onto and is carried through the loader without being applied.
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {
	uint64_t vaddr;
	uint64_t offset;
	uint64_t filesz;
	uint64_t memsz;
	uint32_t flags;
} elf_loadable_segment_t;

typedef enum elf_result : uint8_t {
	ELF_OK = 0,
	ELF_ERR_TOO_SMALL,
	ELF_ERR_BAD_MAGIC,
	ELF_ERR_WRONG_CLASS,
	ELF_ERR_WRONG_ENDIAN,
	ELF_ERR_WRONG_TYPE,
	ELF_ERR_WRONG_MACHINE,
	ELF_ERR_PHDR_OUT_OF_BOUNDS,
	ELF_ERR_TOO_MANY_SEGMENTS,
} elf_result_t;

elf_result_t elf32_parse(const void* data, size_t size, elf_loadable_segment_t* out_segs, int max_segs, int* out_nsegs, uint64_t* out_entry);
int elf_load_segments(addr_space_t addr_space, const void* elf_data, const elf_loadable_segment_t* segs, int nsegs);

#endif