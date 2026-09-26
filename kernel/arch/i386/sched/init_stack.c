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

#define NO_FRAME UINT32_MAX

// the stack's frames are allocated one at a time, so they are not physically
// contiguous and a multi frame argv block cannot be written through a single
// mapping. instead every write is split at frame boundaries and routed to
// whichever frame backs the address, using the one temp mapping window for as
// long as consecutive writes stay inside the same frame.
typedef struct {
	const uint32_t* pages; // physical address of each frame, top frame first
	uint32_t page_count;
	uint32_t virt_top;
	void* window; // temp mapping of the frame under the cursor
	uint32_t window_page; // which frame that is, or NO_FRAME when unmapped
} argv_writer_t;

// returns a kernel-usable pointer for a user address, mapping its frame first
static void* argv_writer_at(argv_writer_t* w, uint32_t user_addr) {
	uint32_t page = (w->virt_top - 1 - user_addr) / PAGE_SIZE;
	if (page >= w->page_count) {
		return nullptr;
	}

	if (w->window_page != page) {
		if (w->window) {
			unmap_physical_temp(w->window, PAGE_SIZE);
			w->window = nullptr;
		}

		w->window = map_physical_temp(w->pages[page], PAGE_SIZE);
		if (!w->window) {
			w->window_page = NO_FRAME;
			return nullptr;
		}

		w->window_page = page;
	}

	return (uint8_t*)w->window + (user_addr & (PAGE_SIZE - 1));
}

// copies len bytes from a kernel address to a user address on the stack
static int argv_writer_put(argv_writer_t* w, uint32_t user_addr, const void* src, size_t len) {
	const uint8_t* s = src;

	while (len > 0) {
		void* dst = argv_writer_at(w, user_addr);
		if (!dst) {
			return -1;
		}

		size_t room = PAGE_SIZE - (user_addr & (PAGE_SIZE - 1));
		size_t chunk = len < room ? len : room;

		memcpy(dst, s, chunk);
		user_addr += chunk;
		s += chunk;
		len -= chunk;
	}

	return 0;
}

static void argv_writer_finish(argv_writer_t* w) {
	if (w->window) {
		unmap_physical_temp(w->window, PAGE_SIZE);
		w->window = nullptr;
		w->window_page = NO_FRAME;
	}
}

int task_build_user_argv_stack(
	const uint32_t* stack_pages,
	uint32_t stack_page_count,
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

	if (total_u64 > max_bytes || total_u64 > (uint64_t)stack_page_count * PAGE_SIZE) {
		return -1;
	}

	size_t total_size = (size_t)total_u64;

	uint32_t user_esp = stack_virt_top - total_size;
	uint32_t string_cursor = user_esp + header_size;

	argv_writer_t w;
	w.pages = stack_pages;
	w.page_count = stack_page_count;
	w.virt_top = stack_virt_top;
	w.window = nullptr;
	w.window_page = NO_FRAME;

	uint32_t argc32 = (uint32_t)argc;
	int rc = argv_writer_put(&w, user_esp, &argc32, sizeof(argc32));

	for (int i = 0; rc == 0 && i < argc; i++) {
		uint32_t arg_ptr = string_cursor;
		rc = argv_writer_put(&w, user_esp + sizeof(uint32_t) + (size_t)i * sizeof(uint32_t), &arg_ptr, sizeof(arg_ptr));

		// reuse the length kernel_user_strlen already validated rather than
		// walking the user string again with an unbounded strlen
		size_t len = kernel_user_strlen(argv[i]) + 1;
		if (rc == 0) {
			rc = argv_writer_put(&w, string_cursor, argv[i], len);
		}

		string_cursor += len;
	}

	if (rc == 0) {
		uint32_t terminator = 0;
		uint32_t arg_table_end = user_esp + sizeof(uint32_t) + (size_t)argc * sizeof(uint32_t);
		rc = argv_writer_put(&w, arg_table_end, &terminator, sizeof(terminator));
	}

	argv_writer_finish(&w);

	if (rc != 0) {
		return -1;
	}

	*out_esp = user_esp;
	return 0;
}