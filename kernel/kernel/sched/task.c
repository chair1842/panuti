/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/handle/registry.h"
#include <kernel/handle/fs.h>
#include <kernel/handle/pipe.h>
#include <kernel/sched/task.h>
#include <kernel/klog.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/slab.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel/sched/sched.h>
#include <kernel/elf.h>
#include <panuti/errno.h>

#define MAX_TASKS 64
#define USER_STACK_VIRT_TOP 0xB0000000
#define PAGE_SIZE 0x1000
#define MAX_ARGV_BYTES (TASK_USER_STACK_SIZE - TASK_USER_CONTEXT_BYTES)
#define ELF_READ_MAX (256 * 1024)

static uint32_t task_count = 0;
static task_t tasks[MAX_TASKS] = {0};

static void task_init_default_streams(task_t* t) {
	if (handle_build("/dvc/console", &t->out_streams[0])) {
		t->no_out_streams = 1;
	} else {
		t->no_out_streams = 0;
	}

	if (handle_build("/dvc/kbd/line", &t->in_streams[0])) {
		t->no_in_streams = 1;
	} else {
		t->no_in_streams = 0;
	}
}

// finds a free slot: one that's never been used (state == 0 / TASK_NONE,
// assuming that's the zero-value of task_state_t). A TASK_TERMINATED task
// is a zombie whose parent hasn't reaped it yet — it still holds a live PID
// that task_wait_pid must be able to find, so its slot is not free until
// task_destroy resets it to TASK_NONE.
// Returns NULL if every slot is occupied.
static task_t* task_find_free_slot(void) {
	for (int i = 0; i < MAX_TASKS; i++) {
		if (tasks[i].state == TASK_NONE) {
			return &tasks[i];
		}
	}
	return nullptr;
}

// hands out the lowest pid no live task is using, so pids get recycled instead
// of climbing forever. with at most MAX_TASKS live tasks this keeps every pid
// inside [1, MAX_TASKS], so there is no wraparound case to get wrong.
//
// a TASK_TERMINATED slot still owns its pid until task_destroy clears the
// slot, so a pid is never handed to a new task while a parent could still be
// blocked in wait() on it -- task_find_free_slot keeps zombies out of the free
// pool for exactly that reason. pid 0 is never handed out, so it doubles as
// the failure return.
static pid_t task_alloc_pid(void) {
	for (pid_t candidate = 1; candidate <= MAX_TASKS; candidate++) {
		bool taken = false;

		for (int i = 0; i < MAX_TASKS; i++) {
			if (tasks[i].state != TASK_NONE && tasks[i].pid == candidate) {
				taken = true;
				break;
			}
		}

		if (!taken) {
			return candidate;
		}
	}

	return 0;
}

static task_t* task_alloc_common(void) {
	task_t* t = task_find_free_slot();
	if (!t) {
		return nullptr;
	}

	t->kernel_stack = (uint32_t)vmalloc_pages(TASK_KERNEL_STACK_PAGES);
	if (!t->kernel_stack) {
		return nullptr;
	}

	t->addr_space = memman_create_addr_space();
	if (!t->addr_space) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
		return nullptr;
	}

	// assign the pid before publishing the slot, so task_alloc_pid's scan
	// never sees this slot claiming a pid it is in the middle of picking
	t->pid = task_alloc_pid();
	if (t->pid == 0) {
		// unreachable: a free slot means fewer than MAX_TASKS live tasks, so
		// some pid in [1, MAX_TASKS] is always free
		memman_destroy_addr_space(t->addr_space);
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
		return nullptr;
	}

	t->state = TASK_READY;
	t->cwd = registry_root();
	task_init_default_streams(t);

	return t;
}

task_t* task_create(void (*entry)(void)) {
	if (!entry) {
		return nullptr;
	}

	task_t* t = task_alloc_common();
	if (!t) {
		return nullptr;
	}

	task_init_stack(t, entry);
	task_count++;
	sched_add(t);

	return t;
}

// a user stack is a run of frames ending at USER_STACK_VIRT_TOP, growing down.
// the frames are allocated one at a time and mapped individually, so they do
// not need to be physically contiguous. vmm_destroy_page_dir frees every frame
// it finds mapped when the address space is torn down, so a partially built
// stack needs no unwinding of its own.
typedef struct {
	uint32_t esp; // initial user esp, just below the top of the stack
	uint32_t pages[TASK_USER_STACK_PAGES]; // physical address of each frame, top frame first
} user_stack_t;

static int user_stack_setup(addr_space_t as, user_stack_t* out) {
	uint32_t base = USER_STACK_VIRT_TOP - TASK_USER_STACK_SIZE;

	out->esp = 0;

	// collect the frames before mapping any of them, so the whole run can go
	// through one cr3 switch instead of two per frame. the flip side is that
	// a short allocation leaves nothing mapped yet, so those frames are freed
	// here rather than left for vmm_destroy_page_dir to find.
	uint32_t phys[TASK_USER_STACK_PAGES];
	uint32_t got = 0;

	for (uint32_t i = 0; i < TASK_USER_STACK_PAGES; i++) {
		phys[i] = memman_alloc_frame();
		if (!phys[i]) {
			break;
		}

		got++;
	}

	if (got != TASK_USER_STACK_PAGES) {
		for (uint32_t i = 0; i < got; i++) {
			memman_free_frame(phys[i]);
		}

		return -1;
	}

	memman_map_in_run(as, base, phys, TASK_USER_STACK_PAGES, MEMMAN_PRESENT | MEMMAN_RW | MEMMAN_USER);

	for (uint32_t i = 0; i < TASK_USER_STACK_PAGES; i++) {
		out->pages[TASK_USER_STACK_PAGES - 1 - i] = phys[i];
	}

	out->esp = USER_STACK_VIRT_TOP - 4;
	return 0;
}

task_t* task_create_user(void (*entry)(void)) {
	task_t* t = task_alloc_common();
	if (!t) {
		return nullptr;
	}

	user_stack_t us;
	if (user_stack_setup(t->addr_space, &us) != 0) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
		memman_destroy_addr_space(t->addr_space);
		t->state = TASK_NONE; // release the slot back, since alloc_common already claimed it
		return nullptr;
	}

	task_init_user_stack(t, entry, us.esp);
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
		return nullptr;
	}

	task_t* t = task_alloc_common();
	if (!t) {
		return nullptr;
	}

	if (elf_load_segments(t->addr_space, elf_data, segs, nsegs) != 0) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
		memman_destroy_addr_space(t->addr_space);
		t->state = TASK_NONE;
		return nullptr;
	}

	user_stack_t us;
	if (user_stack_setup(t->addr_space, &us) != 0) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
		memman_destroy_addr_space(t->addr_space);
		t->state = TASK_NONE;
		return nullptr;
	}

	task_init_user_stack(t, (void (*)(void))(uint32_t)entry, us.esp);
	task_count++;
	sched_add(t);

	return t;
}

// releases one task reference to a stream handle: pipe impls are refcounted
// (they may be shared with children via install_stream), everything else is
// pinned by its inode reference
static void stream_unref(handle_t* h) {
	if (h->type == INODE_PIPE) {
		if (h->impl) {
			pipe_end_unref((pipe_end_t*)h->impl);
		}
	} else if (h->inode) {
		h->inode->refcount--;
	}
}

void task_destroy(task_t* t) {
	if (!t || t->state != TASK_TERMINATED) {
		return;
	}

	for (int i = 0; i < MAX_HANDLES; i++) {
		if (t->handles[i].type != INODE_NONE) {
			inode_unref(t->handles[i].inode);
			if (t->handles[i].ops && t->handles[i].ops->close) {
				t->handles[i].ops->close(t->handles[i].impl, t);
			}
			handle_free(t, i);
		}
	}

	for (int i = 0; i < t->no_in_streams; i++) {
		stream_unref(&t->in_streams[i]);
	}
	for (int i = 0; i < t->no_out_streams; i++) {
		stream_unref(&t->out_streams[i]);
	}

	if (t->kernel_stack) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
	}
	if (t->addr_space) {
		memman_destroy_addr_space(t->addr_space);
	}

	t->esp = 0;
	t->kernel_stack = 0;
	t->addr_space = nullptr;
	t->cwd = nullptr;
	t->next = nullptr;
	t->no_in_streams = 0;
	t->no_out_streams = 0;
	t->state = TASK_NONE;
	task_count--;
}

static void procreate_cleanup(task_t* t) {
	if (!t) {
		return;
	}

	for (int i = 0; i < t->no_in_streams; i++) {
		stream_unref(&t->in_streams[i]);
	}
	for (int i = 0; i < t->no_out_streams; i++) {
		stream_unref(&t->out_streams[i]);
	}

	if (t->kernel_stack) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
	}
	if (t->addr_space) {
		memman_destroy_addr_space(t->addr_space);
	}

	t->state = TASK_NONE;
}

static int elf_read_whole_file(inode_t* n, void** out_data, size_t* out_size) {
	if (!n->mnt->fs_ops->open || !n->mnt->fs_ops->read) {
		return -1;
	}

	// size the buffer from what the filesystem says the file holds, so a small
	// program does not pay to allocate a worst-case one. ELF_READ_MAX still
	// caps the request, so a bogus length cannot become a huge allocation.
	size_t alloc = ELF_READ_MAX;
	if (n->mnt->fs_ops->size) {
		int64_t reported = n->mnt->fs_ops->size(n->mnt->fs_impl, n);

		if (reported > 0 && (uint64_t)reported < (uint64_t)ELF_READ_MAX) {
			alloc = (size_t)reported;
		}
	}

	void* file_impl = n->mnt->fs_ops->open(n->mnt->fs_impl, n);
	if (!file_impl) {
		return -1;
	}

	uint8_t* buf = kmalloc(alloc, 1);
	if (!buf) {
		if (n->mnt->fs_ops->close) {
			n->mnt->fs_ops->close(file_impl);
		}
		return -1;
	}

	size_t total = 0;
	while (total < alloc) {
		int n_read = n->mnt->fs_ops->read(file_impl, buf + total, alloc - total, total);
		if (n_read <= 0) {
			break;
		}
		
		total += (size_t)n_read;
	}

	// a reported length is only ever a hint. if the filesystem handed us a
	// short one we would silently truncate the program, so probe for a byte
	// past the end and grow if there is one, still bounded by ELF_READ_MAX.
	if (total == alloc && alloc < ELF_READ_MAX) {
		uint8_t probe;

		if (n->mnt->fs_ops->read(file_impl, &probe, 1, total) > 0) {
			size_t bigger = alloc * 2;
			if (bigger > ELF_READ_MAX) {
				bigger = ELF_READ_MAX;
			}

			uint8_t* grown = kmalloc(bigger, 1);
			if (grown) {
				memcpy(grown, buf, total);
				kfree(buf);
				buf = grown;
				alloc = bigger;

				while (total < alloc) {
					int n_read = n->mnt->fs_ops->read(file_impl, buf + total, alloc - total, total);
					if (n_read <= 0) {
						break;
					}
					
					total += (size_t)n_read;
				}
			}
		}
	}

	if (total == 0) {
		if (n->mnt->fs_ops->close) {
			n->mnt->fs_ops->close(file_impl);
		}
		kfree(buf);
		return -1;
	}

	if (n->mnt->fs_ops->close) {
		n->mnt->fs_ops->close(file_impl);
	}

	*out_data = buf;
	*out_size = total;
	return 0;
}

static int install_stream(task_t* caller, int fd, handle_t* dest) {
	if (fd < 0 || fd >= MAX_HANDLES || caller->handles[fd].type == INODE_NONE) {
		return -1;
	}

	*dest = caller->handles[fd];
	if (dest->type == INODE_PIPE) {
		/* pipe impls have no inode; keep the shared pipe_end alive instead */
		pipe_end_ref((pipe_end_t*)dest->impl);
	} else if (dest->inode) {
		dest->inode->refcount++;
	}

	return 0;
}

pid_t task_procreate(task_t* caller, const procreate_args_t* args) {
	inode_t* bin = registry_resolve(caller->cwd, args->path);
	if (!bin) {
		return PANUTIERRNO_NOTFOUND;
	}

	void* elf_data;
	size_t elf_size;
	if (elf_read_whole_file(bin, &elf_data, &elf_size) != 0) {
		return PANUTIERRNO_PLAINERR;
	}

	elf_loadable_segment_t segs[16];
	int nsegs;
	uint64_t entry;
	if (elf32_parse(elf_data, elf_size, segs, 16, &nsegs, &entry) != ELF_OK) {
		kfree(elf_data);
		return PANUTIERRNO_PLAINERR;
	}

	task_t* t = task_alloc_common();
	if (!t) {
		kfree(elf_data);
		return PANUTIERRNO_NOFDS;
	}

	t->cwd = caller->cwd;

	if (elf_load_segments(t->addr_space, elf_data, segs, nsegs) != 0) {
		kfree(elf_data);
		procreate_cleanup(t);
		return PANUTIERRNO_PLAINERR;
	}
	
	kfree(elf_data);

	user_stack_t us;
	if (user_stack_setup(t->addr_space, &us) != 0) {
		procreate_cleanup(t);
		return PANUTIERRNO_PLAINERR;
	}

	char* synth_argv[1];
	char** real_argv = args->argv;
	int real_argc = args->argc;
	if (real_argc == 0) {
		synth_argv[0] = (char*)args->path;
		real_argv = synth_argv;
		real_argc = 1;
	}

	uint32_t user_esp;
	if (task_build_user_argv_stack(us.pages, TASK_USER_STACK_PAGES, USER_STACK_VIRT_TOP, real_argv, real_argc, MAX_ARGV_BYTES, &user_esp) != 0) {
		procreate_cleanup(t);
		return PANUTIERRNO_PLAINERR;
	}

	if (args->no_in_streams > 0) {
		for (int i = 0; i < args->no_in_streams; i++) {
			if (install_stream(caller, args->in_streams[i], &t->in_streams[i]) != 0) {
				procreate_cleanup(t);
				return PANUTIERRNO_BADFD;
			}
		}
		
		t->no_in_streams = args->no_in_streams;
	}

	if (args->no_out_streams > 0) {
		for (int i = 0; i < args->no_out_streams; i++) {
			if (install_stream(caller, args->out_streams[i], &t->out_streams[i]) != 0) {
				procreate_cleanup(t);
				return PANUTIERRNO_BADFD;
			}
		}
		
		t->no_out_streams = args->no_out_streams;
	}

	task_init_user_stack(t, (void (*)(void))(uint32_t)entry, user_esp);
	task_count++;
	sched_add(t);

	return t->pid;
}

int task_wait_pid(pid_t target, int* exit_code_out) {
	for (int i = 0; i < MAX_TASKS; i++) {
		if (tasks[i].pid == target && tasks[i].state != TASK_NONE) {
			if (tasks[i].state == TASK_TERMINATED) {
				*exit_code_out = tasks[i].exit_code;
				sched_remove(&tasks[i]);
				task_destroy(&tasks[i]);
				return 0;
			}
			
			return 1;
		}
	}
	
	return -1;
}

void task_wake_waiters(pid_t exited_pid) {
	for (int i = 0; i < MAX_TASKS; i++) {
		if (tasks[i].state == TASK_BLOCKED && tasks[i].pid_waiting_on == exited_pid) {
			tasks[i].pid_waiting_on = 0;
			task_wake(&tasks[i]);
		}
	}
}