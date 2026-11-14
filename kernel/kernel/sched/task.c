/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/handle/registry.h"
#include <kernel/handle/fs.h>
#include <kernel/handle/pipe.h>
#include <kernel/proc/proc.h>
#include <kernel/sched/task.h>
#include <kernel/klog.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/slab.h>
#include <kernel/mem/usr.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel/sched/sched.h>
#include <kernel/irq.h>
#include <kernel/elf.h>
#include <panuti/errno.h>

#define USER_STACK_VIRT_TOP 0xB0000000
#define PAGE_SIZE 0x1000
#define MAX_ARGV_BYTES (TASK_USER_STACK_SIZE - TASK_USER_CONTEXT_BYTES)
#define ELF_READ_MAX (256 * 1024)

static task_t* task_list = nullptr;

static void task_list_push(task_t* t) {
	t->task_next = task_list;
	task_list = t;
	proc_task_add(t);
}

static void task_list_unlink(task_t* t) {
	task_t** pp = &task_list;
	while (*pp) {
		if (*pp == t) {
			*pp = t->task_next;
			break;
		}
		
		pp = &(*pp)->task_next;
	}

	proc_task_remove(t->pid);
	t->task_next = nullptr;
}

static void task_teardown(task_t* t);

static void task_init_default_streams(task_t* t) {
	// a recycled slot still holds the handle_t copies the previous task left
	// in its stream arrays. those ends were already released by
	// task_destroy(), and install_stream() unrefs whatever it is handed
	// before overwriting it -- so leaving them in place would release memory
	// that the allocator may already have handed to somebody else. start the
	// slot out with empty stream arrays.
	for (int i = 0; i < MAX_STREAMS; i++) {
		t->in_streams[i] = (handle_t){0};
		t->out_streams[i] = (handle_t){0};
	}

	t->no_out_streams = 0;
	t->no_in_streams = 0;

	if (handle_build("/dvc/console", &t->out_streams[0])) {
		t->no_out_streams = 1;
	}

	if (handle_build("/dvc/kbd/line", &t->in_streams[0])) {
		t->no_in_streams = 1;
	}
}

// true if some live task has published that it is blocked waiting on `pid`.
// syshandler_wait() registers pid_waiting_on *before* it calls
// task_wait_pid(), so this is the authoritative test for "somebody is about
// to collect this task's exit code", and it holds from the moment a waiter
// commits until it clears the field on the way out.
static bool task_has_waiter(pid_t pid) {
	for (task_t* t = task_list; t; t = t->task_next) {
		if (t->state != TASK_NONE && t->pid_waiting_on == pid) {
			return true;
		}
	}
	return false;
}

static task_t* task_alloc_task(void) {
	for (task_t* t = task_list; t; ) {
		task_t* next = t->task_next;

		if (t->state == TASK_TERMINATED && !task_has_waiter(t->pid)) {
			sched_remove(t);
			task_teardown(t);
			task_list_unlink(t);
			return t;
		}

		t = next;
	}

	return kmalloc(sizeof(task_t), alignof(task_t));
}

static pid_t task_alloc_pid(void) {
	for (pid_t candidate = 1; ; candidate++) {
		bool taken = false;

		for (task_t* t = task_list; t; t = t->task_next) {
			if (t->state != TASK_NONE && t->pid == candidate) {
				taken = true;
				break;
			}
		}

		if (!taken) {
			return candidate;
		}
	}
}

void task_set_cwd(task_t* t, inode_t* cwd) {
	if (!t) {
		return;
	}

	inode_ref(cwd);
	inode_t* old = t->cwd;
	t->cwd = cwd;
	inode_unref(old);
}

void task_set_name(task_t* t, const char* name) {
	if (!t) {
		return;
	}

	size_t len = name ? strlen(name) : 0;
	if (len >= TASK_NAME_MAX) {
		len = TASK_NAME_MAX - 1;
	}

	if (len) {
		memcpy(t->name, name, len);
	}
	
	t->name[len] = '\0';
}

static int task_set_cmdline(task_t* t, char** argv, int argc) {
	size_t pos = 0;

	t->cmdline[0] = '\0';

	for (int i = 0; i < argc; i++) {
		size_t len = kernel_user_strlen(argv[i]);
		if (len == (size_t)-1) {
			return -1;
		}

		if (i > 0 && pos < TASK_CMDLINE_MAX - 1) {
			t->cmdline[pos++] = ' ';
		}

		size_t room = TASK_CMDLINE_MAX - 1 - pos;
		size_t copy = (len < room) ? len : room;
		if (copy) {
			memcpy(t->cmdline + pos, argv[i], copy);
			pos += copy;
		}

		t->cmdline[pos] = '\0';

		if (pos == TASK_CMDLINE_MAX - 1) {
			break;
		}
	}

	return 0;
}

static task_t* task_alloc_common(void) {
	uint32_t flags = irq_save_disable();

	task_t* t = task_alloc_task();
	if (!t) {
		irq_restore(flags);
		return nullptr;
	}

	t->pid = task_alloc_pid();
	if (t->pid == 0) {
		irq_restore(flags);
		task_list_unlink(t);
		kfree(t);
		return nullptr;
	}

	t->state = TASK_READY;
	t->wake_at = 0;
	t->creator_pid = 0;
	t->name[0] = '\0';
	t->cmdline[0] = '\0';
	t->kill_pending = false;
	t->killable = true;

	irq_restore(flags);

	t->kernel_stack = (uint32_t)vmalloc_pages(TASK_KERNEL_STACK_PAGES);
	if (!t->kernel_stack) {
		task_list_unlink(t);
		kfree(t);
		return nullptr;
	}

	t->addr_space = memman_create_addr_space();
	if (!t->addr_space) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
		task_list_unlink(t);
		kfree(t);
		return nullptr;
	}

	task_set_cwd(t, registry_root());
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

	t->killable = false;

	task_init_stack(t, entry);
	task_list_push(t);
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
		task_list_unlink(t);
		kfree(t);
		return nullptr;
	}

	task_init_user_stack(t, entry, us.esp);
	task_list_push(t);
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
		task_list_unlink(t);
		kfree(t);
		return nullptr;
	}

	user_stack_t us;
	if (user_stack_setup(t->addr_space, &us) != 0) {
		vmalloc_free_pages((void*)t->kernel_stack, TASK_KERNEL_STACK_PAGES);
		memman_destroy_addr_space(t->addr_space);
		task_list_unlink(t);
		kfree(t);
		return nullptr;
	}

	task_init_user_stack(t, (void (*)(void))(uint32_t)entry, us.esp);
	task_list_push(t);
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
		inode_unref(h->inode);
	}
}

handle_t* task_return_hslot(uint32_t slot, task_t* task) {
	if (!task) {
		return nullptr;
	}

	fdtable_t* fdt = &task->fdtable;
	if (!fdt->table || !fdt->openb || slot >= fdt->cap) {
		return nullptr;
	}
	
	if (!(fdt->openb[slot / 8] & (1u << (slot % 8)))) {
		return nullptr;
	}

	return &fdt->table[slot];
}

static void task_release_handles(task_t* t) {
	fdtable_t* fdt = &t->fdtable;

	for (uint32_t i = 0; fdt->table && fdt->openb && i < fdt->cap; i++) {
		if (!(fdt->openb[i / 8] & (1u << (i % 8)))) {
			continue;
		}

		handle_t* h = &fdt->table[i];
		if (h->ops && h->ops->close) {
			h->ops->close(h->impl, t);
		}
		
		inode_unref(h->inode);

		handle_free(t, (int)i);
	}
}

static void task_teardown(task_t* t) {
	task_release_handles(t);

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
	task_set_cwd(t, nullptr);
	t->next = nullptr;
	t->no_in_streams = 0;
	t->no_out_streams = 0;
	t->state = TASK_NONE;
}

void task_destroy(task_t* t) {
	if (!t || t->state != TASK_TERMINATED) {
		return;
	}

	task_teardown(t);
	task_list_unlink(t);
	kfree(t);
}

static void procreate_cleanup(task_t* t) {
	if (!t) {
		return;
	}

	// a task that never ran cannot normally have open handles, but a partial
	// setup must not be able to strand one now that handles can pin registry
	// state
	task_release_handles(t);

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

	task_set_cwd(t, nullptr);
	t->state = TASK_NONE;
	task_list_unlink(t);
	kfree(t);
}

static int elf_read_whole_file(inode_t* n, void** out_data, size_t* out_size) {
	if (!n->mnt->fs_ops->open || !n->mnt->fs_ops->read) {
		return -1;
	}

	// size the buffer from what the filesystem says the file holds, so a small
	// program does not pay to allocate a worst-case one. ELF_READ_MAX still
	// caps the request, so a bogus length cannot become a huge allocation.
	size_t alloc = ELF_READ_MAX;
	if (n->mnt->fs_ops->getattr) {
		inode_attr_t attr;

		if (n->mnt->fs_ops->getattr(n->mnt->fs_impl, n, &attr) == 0 &&
		    (attr.valid & INODE_ATTR_SIZE) &&
		    attr.size > 0 && (uint64_t)attr.size < (uint64_t)ELF_READ_MAX) {
			alloc = (size_t)attr.size;
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
	handle_t* h = task_return_hslot((uint32_t)fd, caller);
	if (!h) {
		return -1;
	}

	stream_unref(dest);

	*dest = *h;
	if (dest->type == INODE_PIPE) {
		pipe_end_ref((pipe_end_t*)dest->impl);
	} else if (dest->inode) {
		inode_ref(dest->inode);
	}

	return 0;
}

pid_t task_procreate(task_t* caller, const procreate_args_t* args) {
	inode_t* bin = registry_resolve(caller->cwd, args->path);
	if (!bin) {
		return PANUTIERRNO_NOTFOUND;
	}

	if (bin->type != INODE_FILE) {
		return PANUTIERRNO_UNSUPPORTEDOP;
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
		return PANUTIERRNO_NOMEM;
	}

	task_set_cwd(t, caller->cwd);
	t->creator_pid = caller->pid;

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

	size_t name_len = kernel_user_strlen(real_argv[0]);
	if (name_len == (size_t)-1) {
		procreate_cleanup(t);
		return PANUTIERRNO_INVALIDADDR;
	}

	if (name_len >= TASK_NAME_MAX) {
		name_len = TASK_NAME_MAX - 1;
	}

	memcpy(t->name, real_argv[0], name_len);
	t->name[name_len] = '\0';

	if (task_set_cmdline(t, real_argv, real_argc) != 0) {
		procreate_cleanup(t);
		return PANUTIERRNO_INVALIDADDR;
	}

	for (int i = 0; i < args->no_in_streams; i++) {
		if (install_stream(caller, args->in_streams[i], &t->in_streams[i]) != 0) {
			procreate_cleanup(t);
			return PANUTIERRNO_BADFD;
		}
		t->no_in_streams = i + 1;
	}

	for (int i = 0; i < args->no_out_streams; i++) {
		if (install_stream(caller, args->out_streams[i], &t->out_streams[i]) != 0) {
			procreate_cleanup(t);
			return PANUTIERRNO_BADFD;
		}
		t->no_out_streams = i + 1;
	}

	task_init_user_stack(t, (void (*)(void))(uint32_t)entry, user_esp);
	task_list_push(t);
	sched_add(t);

	return t->pid;
}

task_t* task_find(pid_t pid) {
	for (task_t* t = task_list; t; t = t->task_next) {
		if (t->state != TASK_NONE && t->pid == pid) {
			return t;
		}
	}

	return nullptr;
}

int task_wait_pid(pid_t target, int* exit_code_out) {
	for (task_t* t = task_list; t; ) {
		task_t* next = t->task_next;

		if (t->pid == target && t->state != TASK_NONE) {
			if (t->state == TASK_TERMINATED) {
				*exit_code_out = t->exit_code;
				sched_remove(t);
				task_destroy(t);
				return 0;
			}
			
			return 1;
		}

		t = next;
	}
	
	return -1;
}

void task_wake_waiters(pid_t exited_pid) {
	for (task_t* t = task_list; t; ) {
		task_t* next = t->task_next;

		if (t->state == TASK_BLOCKED && t->pid_waiting_on == exited_pid) {
			t->pid_waiting_on = 0;
			task_wake(t);
		}

		t = next;
	}
}

void task_do_exit(uint32_t exit_code) {
	task_t* current = sched_current();
	uint32_t flags = irq_save_disable();

	if (current->kill_pending) {
		exit_code = TASK_KILL_EXIT_CODE;
	}

	current->exit_code = exit_code;
	current->kill_pending = false;
	current->state = TASK_TERMINATED;
	
	task_wake_waiters(current->pid);
	sched_schedule();

	irq_restore(flags);
}

void task_wake_sleepers(int64_t now) {
	for (task_t* t = task_list; t; ) {
		task_t* next = t->task_next;

		if (t->state == TASK_BLOCKED && t->wake_at && t->wake_at <= now) {
			t->wake_at = 0;
			task_wake(t);
		}

		t = next;
	}
}

static uint32_t task_count_open_fds(const task_t* t) {
	const fdtable_t* fdt = &t->fdtable;
	uint32_t count = 0;

	if (!fdt->table || !fdt->openb) {
		return 0;
	}

	for (uint32_t i = 0; i < fdt->cap; i++) {
		if (fdt->openb[i / 8] & (1u << (i % 8))) {
			count++;
		}
	}

	return count;
}

int task_get_info(pid_t pid, task_info_t* out) {
	if (!out) {
		return -1;
	}

	uint32_t flags = irq_save_disable();

	for (task_t* t = task_list; t; t = t->task_next) {
		if (t->pid != pid || t->state == TASK_NONE) {
			continue;
		}

		// gen this is turning real dense ngl
		out->state = t->state;
		out->exit_code = t->exit_code;
		out->pid_waiting_on = t->pid_waiting_on;
		out->wake_at = t->wake_at;
		out->no_in_streams = t->no_in_streams;
		out->no_out_streams = t->no_out_streams;
		out->open_fds = task_count_open_fds(t);
		out->creator_pid = t->creator_pid;
		out->kill_pending = t->kill_pending;

		if (t->addr_space) {
			memman_usage_t usage;
			memman_get_addr_space_usage(t->addr_space, &usage);
			out->vm_pages = usage.rss_pages;
			out->rss_pages = usage.rss_pages;
			out->pt_pages = usage.pt_pages;
		} else {
			out->vm_pages = 0;
			out->rss_pages = 0;
			out->pt_pages = 0;
		}

		out->kstack_pages = t->kernel_stack ? TASK_KERNEL_STACK_PAGES : 0;

		memcpy(out->name, t->name, TASK_NAME_MAX);
		memcpy(out->cmdline, t->cmdline, TASK_CMDLINE_MAX);

		irq_restore(flags);
		return 0;
	}

	irq_restore(flags);
	return -1;
}
