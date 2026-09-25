/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/sched/task.h>
#include <stdint.h>
#include <kernel/memman/tempmap.h>
#include <kernel/mem/usr.h>
#include <string.h>

extern void enter_usermode_trampoline(void);

#define INITIAL_EFLAGS 0x202 // IF=1, bit 1 always set
#define PAGE_SIZE 0x1000

void task_init_stack(task_t* t, void (*entry)(void)) {
	uint32_t* stack_top = (uint32_t*)(t->kernel_stack + TASK_KERNEL_STACK_SIZE);

	*(--stack_top) = (uint32_t)entry; // for ret to "return" to
	// fake ebp, ebx, esi, and edi
	*(--stack_top) = 0;
	*(--stack_top) = 0;
	*(--stack_top) = 0;
	*(--stack_top) = 0;
	// initial EFLAGS so a first-time task never inherits IF=0 from an
	// interrupt context (task_switch_to popfl's this on the way in)
	*(--stack_top) = INITIAL_EFLAGS;

	t->esp = (uint32_t)stack_top;
}

void task_init_user_stack(task_t* t, void (*entry)(void), uint32_t user_esp) {
	uint32_t* stack_top = (uint32_t*)(t->kernel_stack + TASK_KERNEL_STACK_SIZE);

	*(--stack_top) = user_esp;
	*(--stack_top) = (uint32_t)entry;
	*(--stack_top) = (uint32_t)enter_usermode_trampoline;
	*(--stack_top) = 0; // ebp
	*(--stack_top) = 0; // ebx
	*(--stack_top) = 0; // esi
	*(--stack_top) = 0; // edi
	*(--stack_top) = INITIAL_EFLAGS;

	t->esp = (uint32_t)stack_top;
	t->user_stack = user_esp;
}

int task_build_user_argv_stack(
	uint32_t stack_phys,
	uint32_t stack_virt_top,
    char** argv,
    int argc,
    size_t max_bytes,
    uint32_t* out_esp
) {
	size_t header_size = sizeof(uint32_t) + (size_t)(argc + 1) * sizeof(uint32_t);

	// truncation check: argc is bounded by the caller, but a malicious argv
	// count here would otherwise wrap the header size arithmetic
	if (argc < 0 || (uint64_t)header_size > max_bytes) {
		return -1;
	}

	uint64_t total_u64 = header_size;
	for (int i = 0; i < argc; i++) {
		size_t sl = kernel_user_strlen(argv[i]);
		if (sl == (size_t)-1) {
			return -1;
		}
		total_u64 += sl + 1;
	}

	if (total_u64 > max_bytes || total_u64 > PAGE_SIZE) {
		return -1;
	}

	size_t total_size = (size_t)total_u64;

	void* page = map_physical_temp(stack_phys, PAGE_SIZE);
	if (!page) {
		return -1;
	}

	uint32_t content_offset = PAGE_SIZE - total_size;
	uint32_t user_esp = stack_virt_top - total_size;
	uint32_t user_strings_base = user_esp + header_size;

	uint8_t* header_write = (uint8_t*)page + content_offset;
	uint8_t* string_write = (uint8_t*)page + content_offset + header_size;
	uint32_t string_cursor_user = user_strings_base;

	*(uint32_t*)header_write = (uint32_t)argc;
	header_write += sizeof(uint32_t);

	for (int i = 0; i < argc; i++) {
		*(uint32_t*)header_write = string_cursor_user;
		header_write += sizeof(uint32_t);

		size_t len = strlen(argv[i]) + 1;
		memcpy(string_write, argv[i], len);
		string_write += len;
		string_cursor_user += len;
	}

	*(char**)header_write = nullptr; // argv[] NULL terminator

	unmap_physical_temp(page, PAGE_SIZE);

	*out_esp = user_esp;
	return 0;
}