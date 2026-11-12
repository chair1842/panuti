/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/memman/info.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/irq.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/slab.h>
#include <kernel/memman/vmalloc.h>
#include <panuti/errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
	MEMINFO_TOTAL,
	MEMINFO_FREE,
	MEMINFO_USED,
	MEMINFO_REJECTS,
	MEMINFO_DDFREES,
	MEMINFO_KHEAPCAP,
	MEMINFO_KHEAPHW,
	MEMINFO_KHEAPUSED,
	MEMINFO_KHEAPFREE,
	MEMINFO_SLABS,
	MEMINFO_SLABOBJS,
	MEMINFO_BIGALLOCS,
} meminfo_kind_t;

typedef struct {
	const char* path;
	meminfo_kind_t kind;
} meminfo_file_t;

typedef struct {
	char text[16];
	size_t len;
	size_t pos;
} meminfo_handle_t;

static uint32_t meminfo_value(meminfo_kind_t kind) {
	switch (kind) {
	case MEMINFO_TOTAL:
		return memman_total_frames();
	case MEMINFO_FREE:
		return memman_free_frames();
	case MEMINFO_USED: 
		{
			uint32_t flags = irq_save_disable();
			uint32_t total = memman_total_frames();
			uint32_t free = memman_free_frames();
			
			irq_restore(flags);
			return total - free;
		}
	case MEMINFO_REJECTS:
		return memman_rejects();
	case MEMINFO_DDFREES:
		return memman_double_frees();
	case MEMINFO_KHEAPCAP:
	case MEMINFO_KHEAPHW:
	case MEMINFO_KHEAPUSED:
	case MEMINFO_KHEAPFREE: {
		vmalloc_stats_t s;
		vmalloc_get_stats(&s);

		switch (kind) {
		case MEMINFO_KHEAPCAP:
			return s.capacity;
		case MEMINFO_KHEAPHW:
			return s.high_water;
		case MEMINFO_KHEAPUSED:
			return s.in_use;
		default:
			return s.capacity - s.in_use;
		}
	}
	
	case MEMINFO_SLABS:
	case MEMINFO_SLABOBJS:
	case MEMINFO_BIGALLOCS: 
		{
			slab_stats_t s;
			slab_get_stats(&s);

			if (kind == MEMINFO_SLABS) {
				return s.slab_pages;
			}
		
			if (kind == MEMINFO_SLABOBJS) {
				return s.slab_used;
			}
		
			return s.big_allocs;
		}
	}

	return 0;
}

typedef struct {
	char* buf;
	size_t cap;
	size_t len;
} meminfo_sink_t;

static void meminfo_putchar(char c, void* extra) {
	meminfo_sink_t* s = (meminfo_sink_t*)extra;
	if (s->len + 1 < s->cap) {
		s->buf[s->len] = c;
		s->len++;
		s->buf[s->len] = '\0';
	}
}

static size_t meminfo_render(char* buf, size_t cap, uint32_t value) {
	meminfo_sink_t s = { buf, cap, 0 };

	if (cap == 0) {
		return 0;
	}

	buf[0] = '\0';
	fctprintf(meminfo_putchar, &s, "%u\n", value);
	return s.len;
}

static int meminfo_open(void* impl, void** out_impl) {
	const meminfo_file_t* file = (const meminfo_file_t*)impl;

	meminfo_handle_t* h = kmalloc(sizeof(*h), alignof(meminfo_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	h->pos = 0;
	h->len = meminfo_render(h->text, sizeof(h->text), meminfo_value(file->kind));

	*out_impl = h;
	return 0;
}

static int meminfo_read(void* impl, void* buf, size_t len) {
	meminfo_handle_t* h = (meminfo_handle_t*)impl;

	if (h->pos >= h->len) {
		return 0;
	}

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;
	memcpy(buf, h->text + h->pos, n);
	h->pos += n;

	return (int)n;
}

static int meminfo_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t meminfo_ops = {
	.open = meminfo_open,
	.read = meminfo_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.seek = op_seek_ignore,
	.close = meminfo_close,
};

static const meminfo_file_t meminfo_files[] = {
	{ "/sys/meminfo/total", MEMINFO_TOTAL },
	{ "/sys/meminfo/free", MEMINFO_FREE },
	{ "/sys/meminfo/used", MEMINFO_USED },
	{ "/sys/meminfo/rejects", MEMINFO_REJECTS },
	{ "/sys/meminfo/ddfrees", MEMINFO_DDFREES },
	{ "/sys/meminfo/kheapcap", MEMINFO_KHEAPCAP },
	{ "/sys/meminfo/kheaphw", MEMINFO_KHEAPHW },
	{ "/sys/meminfo/kheapused", MEMINFO_KHEAPUSED },
	{ "/sys/meminfo/kheapfree", MEMINFO_KHEAPFREE },
	{ "/sys/meminfo/slabs", MEMINFO_SLABS },
	{ "/sys/meminfo/slabobjs", MEMINFO_SLABOBJS },
	{ "/sys/meminfo/bigallocs", MEMINFO_BIGALLOCS },
};

void meminfo_init(void) {
	registry_mkdir("/sys/meminfo");

	for (size_t i = 0; i < sizeof(meminfo_files) / sizeof(meminfo_files[0]); i++) {
		registry_add(meminfo_files[i].path, INODE_DVC, (void*)&meminfo_files[i], &meminfo_ops);
	}
}
