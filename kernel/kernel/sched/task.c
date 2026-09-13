#include "kernel/handle/registry.h"
#include <kernel/sched/task.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/memman.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/sched/sched.h>
#include <kernel/elf.h>
#include <kernel/handle/handle.h>

#define MAX_TASKS 64
#define USER_STACK_VIRT_TOP 0xB0000000
#define PAGE_SIZE 0x1000

static pid_t next_pid = 1;
static uint32_t task_count = 0;
static task_t tasks[MAX_TASKS] = {0};

static void task_init_default_streams(task_t* t) {
	if (handle_build("/dvc/console", &t->out_streams[0])) {
		t->no_out_streams = 1;
	} else {
		t->no_out_streams = 0;
	}

	if (handle_build("/dvc/kbd", &t->in_streams[0])) {
		t->no_in_streams = 1;
	} else {
		t->no_in_streams = 0;
	}
}

static task_t* task_alloc_common(void) {
	if (task_count == MAX_TASKS) {
		return NULL;
	}
	task_t* t = &tasks[task_count];

	t->kernel_stack = (uint32_t)vmalloc_pg();
	if (!t->kernel_stack) {
		return NULL;
	}

	t->addr_space = memman_create_addr_space();
	if (!t->addr_space) {
		vmalloc_free((void*)t->kernel_stack);
		return NULL;
	}

	t->pid = next_pid++;
	t->state = TASK_READY;
	t->cwd = registry_root();
	task_init_default_streams(t);

	return t;
}

task_t* task_create(void (*entry)(void)) {
	if (!entry) {
		return NULL;
	}

	task_t* t = task_alloc_common();
	if (!t) {
		return NULL;
	}

	task_init_stack(t, entry);
	task_count++;
	sched_add(t);

	return t;
}

task_t* task_create_user(void (*entry)(void)) {
	task_t* t = task_alloc_common();
	if (!t) {
		return NULL;
	}

	uint32_t user_stack_phys = memman_alloc_frame();
	if (!user_stack_phys) {
		vmalloc_free((void*)t->kernel_stack);
		memman_destroy_addr_space(t->addr_space);
		return NULL;
	}

	uint32_t user_stack_virt_base = USER_STACK_VIRT_TOP - PAGE_SIZE;
	memman_map_in(t->addr_space, user_stack_virt_base, user_stack_phys, MEMMAN_PRESENT | MEMMAN_RW | MEMMAN_USER);
	uint32_t user_esp = user_stack_virt_base + PAGE_SIZE;

	task_init_user_stack(t, entry, user_esp);
	task_count++;
	sched_add(t);

	return t;
}

task_t* task_create_frelf_user(const void* elf_data, size_t elf_size) {
	elf_loadable_segment_t segs[16];
	int nsegs;
	uint64_t entry;
	elf_result_t result = elf32_parse(elf_data, elf_size, segs, 16, &nsegs, &entry);
	if (result != ELF_OK) {
		return NULL;
	}

	task_t* t = task_alloc_common();
	if (!t) {
		return NULL;
	}

	if (elf_load_segments(t->addr_space, elf_data, segs, nsegs) != 0) {
		vmalloc_free((void*)t->kernel_stack);
		memman_destroy_addr_space(t->addr_space);
		return NULL;
	}

	uint32_t user_stack_phys = memman_alloc_frame();
	if (!user_stack_phys) {
		vmalloc_free((void*)t->kernel_stack);
		memman_destroy_addr_space(t->addr_space);
		return NULL;
	}

	uint32_t user_stack_virt_base = USER_STACK_VIRT_TOP - PAGE_SIZE;
	memman_map_in(t->addr_space, user_stack_virt_base, user_stack_phys, MEMMAN_PRESENT | MEMMAN_RW | MEMMAN_USER);
	uint32_t user_esp = user_stack_virt_base + PAGE_SIZE;

	task_init_user_stack(t, (void (*)(void))(uint32_t)entry, user_esp);
	task_count++;
	sched_add(t);

	return t;
}

void task_destroy(task_t* t) {
	if (!t || t->state != TASK_TERMINATED) {
		return;
	}

	for (int i = 0; i < MAX_HANDLES; i++) {
		if (t->handles[i].type != INODE_NONE) {
			handle_free(t, i);
		}
	}

	// release the inode refs streams took at creation/spawn time,
	// same as the handle table above
	for (int i = 0; i < t->no_in_streams; i++) {
		if (t->in_streams[i].inode) {
			t->in_streams[i].inode->refcount--;
		}
	}
	for (int i = 0; i < t->no_out_streams; i++) {
		if (t->out_streams[i].inode) {
			t->out_streams[i].inode->refcount--;
		}
	}

	if (t->kernel_stack) {
		vmalloc_free((void*)t->kernel_stack);
	}
	if (t->addr_space) {
		memman_destroy_addr_space(t->addr_space);
	}

	t->esp = 0;
	t->kernel_stack = 0;
	t->addr_space = NULL;
	t->cwd = NULL;
	t->next = NULL;
	t->no_in_streams = 0;
	t->no_out_streams = 0;
	t->state = TASK_TERMINATED;
	task_count--;
}