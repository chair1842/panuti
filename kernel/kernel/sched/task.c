/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/handle/registry.h"
#include <kernel/handle/fs.h>
#include <kernel/handle/pipe.h>
#include <kernel/handle/point.h>
#include <kernel/proc/proc.h>
#include <kernel/sched/task.h>
#include <kernel/klog.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/layout.h>
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

#define MAX_ARGV_BYTES (TASK_USER_STACK_SIZE - TASK_USER_CONTEXT_BYTES)
#define ELF_READ_MAX (256 * 1024)
#define SHEBANG_TOKEN_MAX 256

static task_t* task_list = nullptr;
static task_t* task_free_list = nullptr;

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

static bool task_waits_on(const task_t* t, pid_t pid) {
	for (uint32_t i = 0; i < t->wait_pid_count; i++) {
		if (t->wait_pids[i] == pid) {
			return true;
		}
	}

	return false;
}

static task_t* task_alloc_task(void) {
	if (task_free_list) {
		task_t* t = task_free_list;
		task_free_list = t->task_next;
		t->task_next = nullptr;
		return t;
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
		size_t len = kernel_arg_strlen(argv[i]);
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
	t->exit_code = 0;
	t->wake_at = 0;
	t->exit_point = nullptr;
	t->wait_pid_count = 0;
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
	t->exit_point = nullptr;
	t->no_in_streams = 0;
	t->no_out_streams = 0;
	t->state = TASK_NONE;
}

static void task_reap(task_t* t) {
	if (!t || t->state != TASK_TERMINATED) {
		return;
	}

	task_teardown(t);
	task_list_unlink(t);

	// keep the cleaned-out slot for task_alloc_task instead of freeing it
	t->task_next = task_free_list;
	task_free_list = t;
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

	if (h->type == INODE_POINT) {
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

static bool shebang_space(uint8_t c) {
	return c == ' ' || c == '\t';
}

static int shebang_parse(const void* data, size_t size, char** interp_out, char** arg_out) {
	const uint8_t* p = data;
	if (size < 2 || p[0] != '#' || p[1] != '!') {
		return 0;
	}

	size_t i = 2;
	while (i < size && shebang_space(p[i])) {
		i++;
	}

	size_t interp_start = i;
	while (i < size && !shebang_space(p[i]) && p[i] != '\n' && p[i] != '\r' && p[i] != '\0') {
		i++;
	}

	size_t interp_len = i - interp_start;
	if (interp_len == 0 || interp_len > SHEBANG_TOKEN_MAX) {
		return -1;
	}

	while (i < size && shebang_space(p[i])) {
		i++;
	}

	size_t arg_start = i;
	while (i < size && p[i] != '\n' && p[i] != '\r' && p[i] != '\0') {
		i++;
	}

	if (i < size && p[i] == '\0') {
		return -1;
	}

	size_t arg_end = i;
	while (arg_end > arg_start && shebang_space(p[arg_end - 1])) {
		arg_end--;
	}

	size_t arg_len = arg_end - arg_start;
	if (arg_len > SHEBANG_TOKEN_MAX) {
		return -1;
	}

	char* interp = kmalloc(interp_len + 1, 1);
	if (!interp) {
		return -1;
	}
	memcpy(interp, p + interp_start, interp_len);
	interp[interp_len] = '\0';

	char* arg = nullptr;
	if (arg_len != 0) {
		arg = kmalloc(arg_len + 1, 1);
		if (!arg) {
			kfree(interp);
			return -1;
		}
		memcpy(arg, p + arg_start, arg_len);
		arg[arg_len] = '\0';
	}

	*interp_out = interp;
	*arg_out = arg;
	return 1;
}

pid_t task_procreate(task_t* caller, const procreate_args_t* args) {
	inode_t* bin = registry_resolve(caller->cwd, args->path);
	if (!bin) {
		return PANUTIERRNO_NOTFOUND;
	}

	if (bin->type != INODE_FILE) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	int rc = PANUTIERRNO_PLAINERR;
	void* elf_data = nullptr;
	size_t elf_size = 0;
	char* shebang_interp = nullptr;
	char* shebang_arg = nullptr;
	task_t* t = nullptr;
	char** exec_argv = nullptr;
	char* synth_argv[1];
	bool exec_argv_owned = false;
	int exec_argc = 0;
	uint64_t entry = 0;
	elf_loadable_segment_t segs[16];
	int nsegs = 0;

	if (elf_read_whole_file(bin, &elf_data, &elf_size) != 0) {
		return PANUTIERRNO_PLAINERR;
	}

	int has_shebang = shebang_parse(elf_data, elf_size, &shebang_interp, &shebang_arg);
	if (has_shebang < 0) {
		goto fail_data;
	}

	if (has_shebang > 0) {
		inode_t* interp = registry_resolve(caller->cwd, shebang_interp);
		if (!interp) {
			rc = PANUTIERRNO_NOTFOUND;
			goto fail_data;
		}

		if (interp->type != INODE_FILE) {
			rc = PANUTIERRNO_UNSUPPORTEDOP;
			goto fail_data;
		}

		kfree(elf_data);
		elf_data = nullptr;

		if (elf_read_whole_file(interp, &elf_data, &elf_size) != 0) {
			goto fail_data;
		}
	}

	if (elf32_parse(elf_data, elf_size, segs, 16, &nsegs, &entry) != ELF_OK) {
		goto fail_data;
	}

	t = task_alloc_common();
	if (!t) {
		rc = PANUTIERRNO_NOMEM;
		goto fail_data;
	}

	task_set_cwd(t, caller->cwd);
	t->creator_pid = caller->pid;

	if (elf_load_segments(t->addr_space, elf_data, segs, nsegs) != 0) {
		goto fail_task;
	}

	kfree(elf_data);
	elf_data = nullptr;

	user_stack_t us;
	if (user_stack_setup(t->addr_space, &us) != 0) {
		goto fail_task;
	}

	if (has_shebang > 0) {
		exec_argc = 2 + (shebang_arg ? 1 : 0) + (args->argc > 0 ? args->argc - 1 : 0);

		if (exec_argc > MAX_ARGV_COUNT) {
			goto fail_task;
		}

		char** new_argv = kmalloc(sizeof(*new_argv) * (size_t)exec_argc, 1);
		if (!new_argv) {
			rc = PANUTIERRNO_NOMEM;
			goto fail_task;
		}

		int k = 0;
		new_argv[k++] = shebang_interp;
		if (shebang_arg) {
			new_argv[k++] = shebang_arg;
		}
		
		new_argv[k++] = (char*)args->path;
		for (int i = 1; i < args->argc; i++) {
			new_argv[k++] = args->argv[i];
		}

		exec_argv = new_argv;
		exec_argv_owned = true;
	} else if (args->argc == 0) {
		synth_argv[0] = (char*)args->path;
		exec_argv = synth_argv;
		exec_argc = 1;
	} else {
		exec_argv = args->argv;
		exec_argc = args->argc;
	}

	uint32_t user_esp;
	if (task_build_user_argv_stack(us.pages, TASK_USER_STACK_PAGES, USER_STACK_VIRT_TOP, exec_argv, exec_argc, MAX_ARGV_BYTES, &user_esp) != 0) {
		goto fail_task;
	}

	size_t name_len = kernel_arg_strlen(exec_argv[0]);
	if (name_len == (size_t)-1) {
		rc = PANUTIERRNO_INVALIDADDR;
		goto fail_task;
	}

	if (name_len >= TASK_NAME_MAX) {
		name_len = TASK_NAME_MAX - 1;
	}

	memcpy(t->name, exec_argv[0], name_len);
	t->name[name_len] = '\0';

	if (task_set_cmdline(t, exec_argv, exec_argc) != 0) {
		rc = PANUTIERRNO_INVALIDADDR;
		goto fail_task;
	}

	for (int i = 0; i < args->no_in_streams; i++) {
		if (install_stream(caller, args->in_streams[i], &t->in_streams[i]) != 0) {
			rc = PANUTIERRNO_BADFD;
			goto fail_task;
		}
		t->no_in_streams = i + 1;
	}

	for (int i = 0; i < args->no_out_streams; i++) {
		if (install_stream(caller, args->out_streams[i], &t->out_streams[i]) != 0) {
			rc = PANUTIERRNO_BADFD;
			goto fail_task;
		}
		t->no_out_streams = i + 1;
	}

	if (exec_argv_owned) {
		kfree(exec_argv);
	}
	kfree(shebang_interp);
	kfree(shebang_arg);

	task_init_user_stack(t, (void (*)(void))(uint32_t)entry, user_esp);
	task_list_push(t);
	sched_add(t);

	return t->pid;

fail_task:
	kfree(elf_data);
	if (exec_argv_owned) {
		kfree(exec_argv);
	}
	kfree(shebang_interp);
	kfree(shebang_arg);
	procreate_cleanup(t);
	return rc;

fail_data:
	kfree(elf_data);
	kfree(shebang_interp);
	kfree(shebang_arg);
	return rc;
}

task_t* task_find(pid_t pid) {
	for (task_t* t = task_list; t; t = t->task_next) {
		if (t->state != TASK_NONE && t->pid == pid) {
			return t;
		}
	}

	return nullptr;
}

int task_wait_any(const pid_t* pids, uint32_t count, int* exit_code_out, pid_t* exit_pid_out) {
	bool alive = false;

	for (task_t* t = task_list; t; ) {
		task_t* next = t->task_next;

		bool wanted = false;
		for (uint32_t i = 0; i < count; i++) {
			if (pids[i] == t->pid) {
				wanted = true;
				break;
			}
		}

		if (wanted && t->state != TASK_NONE) {
			if (t->state == TASK_TERMINATED) {
				*exit_code_out = t->exit_code;
				*exit_pid_out = t->pid;
				sched_remove(t);
				task_reap(t);
				return 0;
			}

			alive = true;
		}

		t = next;
	}
	
	return alive ? 1 : -1;
}

void task_wake_waiters(pid_t exited_pid) {
	for (task_t* t = task_list; t; ) {
		task_t* next = t->task_next;

		if (t->state == TASK_BLOCKED && task_waits_on(t, exited_pid)) {
			task_wake(t);
		}

		t = next;
	}
}

void task_do_exit(uint32_t exit_code) {
	task_t* current = sched_current();
	uint32_t flags = irq_save_disable();

	if (!current) {
		irq_restore(flags);
		return;
	}

	if (current->kill_pending) {
		exit_code = TASK_KILL_EXIT_CODE;
	}

	current->exit_code = exit_code;
	current->kill_pending = false;
	current->state = TASK_TERMINATED;

	if (current->exit_point) {
		point_fire(current->exit_point);
		current->exit_point = nullptr;
	}
	
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
		out->pid_waiting_on = t->wait_pid_count ? t->wait_pids[0] : 0;
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
