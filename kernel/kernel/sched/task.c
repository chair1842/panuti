#include "kernel/handle/registry.h"
#include <kernel/handle/fs.h>
#include <kernel/sched/task.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/slab.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/sched/sched.h>
#include <kernel/elf.h>
#include <panuti/errno.h>

#define MAX_TASKS 64
#define USER_STACK_VIRT_TOP 0xB0000000
#define PAGE_SIZE 0x1000
#define MAX_ARGV_BYTES 512
#define ELF_READ_MAX (256 * 1024)

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

// finds a free slot: one that's never been used (state == 0 / TASK_NONE,
// assuming that's the zero-value of task_state_t) or has been fully
// reaped (TASK_TERMINATED with everything already torn down by
// task_destroy). Returns NULL if every slot is occupied.
static task_t* task_find_free_slot(void) {
	for (int i = 0; i < MAX_TASKS; i++) {
		if (tasks[i].state == TASK_NONE || tasks[i].state == TASK_TERMINATED) {
			return &tasks[i];
		}
	}
	return NULL;
}

static task_t* task_alloc_common(void) {
	task_t* t = task_find_free_slot();
	if (!t) {
		return NULL;
	}

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
		t->state = TASK_NONE; // release the slot back, since alloc_common already claimed it
		return NULL;
	}

	uint32_t user_stack_virt_base = USER_STACK_VIRT_TOP - PAGE_SIZE;
	memman_map_in(t->addr_space, user_stack_virt_base, user_stack_phys, MEMMAN_PRESENT | MEMMAN_RW | MEMMAN_USER);
	uint32_t user_esp = user_stack_virt_base + PAGE_SIZE - 4;

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
		t->state = TASK_NONE;
		return NULL;
	}

	uint32_t user_stack_phys = memman_alloc_frame();
	if (!user_stack_phys) {
		vmalloc_free((void*)t->kernel_stack);
		memman_destroy_addr_space(t->addr_space);
		t->state = TASK_NONE;
		return NULL;
	}

	uint32_t user_stack_virt_base = USER_STACK_VIRT_TOP - PAGE_SIZE;
	memman_map_in(t->addr_space, user_stack_virt_base, user_stack_phys, MEMMAN_PRESENT | MEMMAN_RW | MEMMAN_USER);
	uint32_t user_esp = user_stack_virt_base + PAGE_SIZE - 4;

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
			inode_unref(t->handles[i].inode);
			if (t->handles[i].ops && t->handles[i].ops->close) {
				t->handles[i].ops->close(t->handles[i].impl, t);
			}
			handle_free(t, i);
		}
	}

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
	t->state = TASK_NONE;
	task_count--;
}

static void procreate_cleanup(task_t* t) {
	if (!t) {
		return;
	}

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

	t->state = TASK_NONE;
}

static int elf_read_whole_file(inode_t* n, void** out_data, size_t* out_size) {
	if (!n->mnt->fs_ops->open) {
		return -1;
	}

	void* file_impl = n->mnt->fs_ops->open(n->mnt->fs_impl, n);
	if (!file_impl) {
		return -1;
	}

	uint8_t* buf = kmalloc(ELF_READ_MAX, 1);
	if (!buf) {
		if (n->mnt->fs_ops->close) {
			n->mnt->fs_ops->close(file_impl);
		}
		return -1;
	}

	size_t total = 0;
	while (total < ELF_READ_MAX) {
		int n_read = n->mnt->fs_ops->read(file_impl, buf + total, ELF_READ_MAX - total, total);
		if (n_read <= 0) {
			break;
		}
		
		total += (size_t)n_read;
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
	if (dest->inode) {
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

	if (elf_load_segments(t->addr_space, elf_data, segs, nsegs) != 0) {
		kfree(elf_data);
		procreate_cleanup(t);
		return PANUTIERRNO_PLAINERR;
	}
	
	kfree(elf_data);

	uint32_t user_stack_phys = memman_alloc_frame();
	if (!user_stack_phys) {
		procreate_cleanup(t);
		return PANUTIERRNO_PLAINERR;
	}

	uint32_t user_stack_virt_base = USER_STACK_VIRT_TOP - PAGE_SIZE;
	memman_map_in(t->addr_space, user_stack_virt_base, user_stack_phys, MEMMAN_PRESENT | MEMMAN_RW | MEMMAN_USER);

	char* synth_argv[1];
	char** real_argv = args->argv;
	int real_argc = args->argc;
	if (real_argc == 0) {
		synth_argv[0] = (char*)args->path;
		real_argv = synth_argv;
		real_argc = 1;
	}

	uint32_t user_esp;
	if (task_build_user_argv_stack(user_stack_phys, USER_STACK_VIRT_TOP, real_argv, real_argc, MAX_ARGV_BYTES, &user_esp) != 0) {
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