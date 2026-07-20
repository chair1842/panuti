#include <kernel/elf.h>
#include <stdint.h>

#define EDHR_MACHINE 3 // EM_386

typedef uint32_t elf32_offset_t;
typedef uint32_t elf32_addr_t;

typedef struct {
	uint8_t	ident[ELF_NIDENT];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	elf32_addr_t entry;
	elf32_offset_t phoff;
	elf32_offset_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phnum;
	uint16_t shentsize;
	uint16_t shnum;
	uint16_t shstrndx;
} elf32_edhr_t;

typedef struct {
	uint32_t type;
	elf32_offset_t offset;
	elf32_addr_t vaddr;
	elf32_addr_t paddr;
	uint32_t filesz;
	uint32_t memsz;
	uint32_t flags;
	uint32_t align;
} elf32_phdr_t;

elf_result_t elf32_parse(const void* data, size_t size, elf_loadable_segment_t* out_segs, int max_segs, int* out_nsegs, uint64_t* out_entry) {
	if (size < sizeof(elf32_edhr_t)) {
		return ELF_ERR_TOO_SMALL;
	}
	

	const elf32_edhr_t* ehdr = (const elf32_edhr_t*)data;
	if (ehdr->ident[0] != 0x7F || ehdr->ident[1] != 'E' || ehdr->ident[2] != 'L'  || ehdr->ident[3] != 'F') {
		return ELF_ERR_BAD_MAGIC;
	}
	

	if (ehdr->ident[4] != 1) {
		return ELF_ERR_WRONG_ENDIAN;
	}

	if (ehdr->ident[5] != 1) {
		return ELF_ERR_WRONG_CLASS;
	}

	if (ehdr->type != 2) {
		return ELF_ERR_WRONG_TYPE;
	}

	if (ehdr->machine != EDHR_MACHINE) {
		return ELF_ERR_WRONG_MACHINE;
	}
	

	uint64_t phdr_table_end = (uint64_t)ehdr->phoff + (uint64_t)ehdr->phentsize * (uint64_t)ehdr->phnum;
	if (phdr_table_end > size) {
		return ELF_ERR_PHDR_OUT_OF_BOUNDS;
	}
	

	const uint8_t* phdr_base = (const uint8_t*)data + ehdr->phoff;
	int nsegs = 0;

	for (uint16_t i = 0; i < ehdr->phnum; i++) {
		const elf32_phdr_t* phdr = (const elf32_phdr_t*)(phdr_base + i * ehdr->phentsize);
		if (phdr->type != 1) {
			continue; // skip anything that isn't PT_LOAD
		}

		if (nsegs >= max_segs) {
			return ELF_ERR_TOO_MANY_SEGMENTS;
		}

		out_segs[nsegs].vaddr  = phdr->vaddr;
		out_segs[nsegs].offset = phdr->offset;
		out_segs[nsegs].filesz = phdr->filesz;
		out_segs[nsegs].memsz  = phdr->memsz;
		out_segs[nsegs].flags  = phdr->flags;
		nsegs++;
	}


	*out_nsegs = nsegs;
	*out_entry = ehdr->entry;
	return ELF_OK;
}

