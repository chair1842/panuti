// slab.c
#include "kernel/klog.h"
#include "kernel/tty.h"
#include <kernel/memman/slab.h>
#include <kernel/memman/vmalloc.h>
#include <string.h>
#include <kernel/kpanic.h>

#define SLAB_MAGIC 0xBABEBAB1 // ha, babe baby eating dead beef

typedef struct slab slab_t;
struct slab {
	uint32_t magic;
	uint32_t obj_size;
	uint32_t free;      // index of first free object, -1 if none
	uint32_t used;
	uint32_t capacity;
	struct slab* next;
};

typedef struct kmallocCache kmallocCache_t;
struct kmallocCache{
	uint32_t obj_size;
	slab_t* slabs;
};

static kmallocCache_t caches[] = {
	{ 8, NULL },
	{ 16, NULL },
	{ 32, NULL },
	{ 64, NULL },
	{ 128, NULL },
	{ 256, NULL },
	{ 512, NULL },
	{ 1024, NULL },
	{ 2048, NULL },
};

#define NUM_CACHES (sizeof(caches) / sizeof(caches[0]))
#define PAGE_SIZE 4096

static uint32_t align_up(uint32_t value, uint32_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

// hmm claudgpt generated code.
static uint8_t* slab_obj(slab_t* slab, uint32_t index) {
	return (uint8_t*)slab + align_up(sizeof(slab_t), slab->obj_size) + index * slab->obj_size;
}

static slab_t* slab_create(uint32_t obj_size) {
	slab_t* slab = (slab_t*)vmalloc_pg();
	klog(KLOG_INFO, "slab_create: vmalloc_pg returned\n");
	if (!slab) {
		kpanic("slab_create: vmalloc_pg returned NULL\n");
		return NULL;
	}

	slab->magic = SLAB_MAGIC;
	slab->obj_size = obj_size;
	slab->used = 0;
	slab->next = NULL;

	uint32_t header_size = align_up(sizeof(slab_t), obj_size);
	slab->capacity = (PAGE_SIZE - header_size) / obj_size;

	for (uint32_t i = 0; i < slab->capacity - 1; i++) {
		uint32_t* obj = (uint32_t*)slab_obj(slab, i);
		*obj = i + 1;
	}
	uint32_t* last = (uint32_t*)slab_obj(slab, slab->capacity - 1);
	*last = (uint32_t)-1;

	slab->free = 0;
	return slab;
}

static kmallocCache_t* cache_for(uint32_t size, uint32_t align) {
	for (uint32_t i = 0; i < NUM_CACHES; i++) {
		if (caches[i].obj_size >= size && caches[i].obj_size % align == 0) {
			return &caches[i];
		}
	}
	return NULL;
}

void* kmalloc(uint32_t size, uint32_t align) {
	if (size == 0) {
		return NULL;
	}

	if (align == 0) {
		align = 1;
	}
	
	if (size > 2048) {
		if (size > 4096) {
			return NULL;
		}
		
		return vmalloc_pg();
	}

	kmallocCache_t* cache = cache_for(size, align);
	if (!cache) {
		return NULL;
	}

	slab_t* slab = cache->slabs;
	while (slab && slab->free == (uint32_t)-1) {
		slab = slab->next;
	}

	if (!slab) {
		slab = slab_create(cache->obj_size);
		if (!slab) {
			return NULL;
		}

		slab->next   = cache->slabs;
		cache->slabs = slab;
	}

	uint32_t index = slab->free;
	uint32_t* obj = (uint32_t*)slab_obj(slab, index);
	slab->free = *obj;
	slab->used++;

	memset(obj, 0, slab->obj_size);
	return (void*)obj;
}

void kfree(void* ptr) {
	if (!ptr) {
		return;
	}

	slab_t* slab = (slab_t*)((uint32_t)ptr & ~0xFFF);

	if (slab->magic != SLAB_MAGIC) {
		if (((uint32_t)ptr & (PAGE_SIZE - 1)) == 0) {
			vmalloc_free(ptr);
		}
		return;
	}

	uint32_t offset = (uint8_t*)ptr - (uint8_t*)slab - align_up(sizeof(slab_t), slab->obj_size);
	if (offset % slab->obj_size != 0) {
		return;
	}
	uint32_t index = offset / slab->obj_size;
	if (index >= slab->capacity || slab->used == 0) {
		return;
	}

	for (uint32_t free_index = slab->free, count = 0;
		 free_index != (uint32_t)-1 && count < slab->capacity;
		 free_index = *(uint32_t*)slab_obj(slab, free_index), count++) {
		if (free_index == index) {
			return;
		}
	}

	uint32_t* obj = (uint32_t*)ptr;
	*obj = slab->free;
	slab->free = index;
	slab->used--;
}
