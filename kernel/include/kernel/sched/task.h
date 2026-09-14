#ifndef _KERNEL_SCHED_TASK_H
#define _KERNEL_SCHED_TASK_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <panuti/syscall/procreate.h>

#define TASK_KERNEL_STACK_SIZE 4096
#define MAX_STREAMS 16

typedef uint32_t pid_t;

typedef enum task_state {
	TASK_NONE = 0,
	TASK_READY,
	TASK_RUNNING,
	TASK_BLOCKED,
	TASK_TERMINATED,
} task_state_t;

typedef struct task {
	uint32_t esp;
	uint32_t kernel_stack;
	uint32_t user_stack;
	void* addr_space;
	
	task_state_t state;
	
	pid_t pid;
	
	struct task* next;
	
	handle_t handles[MAX_HANDLES];
	inode_t* cwd;

	handle_t in_streams[MAX_STREAMS];
	int no_in_streams;

	handle_t out_streams[MAX_STREAMS];
	int no_out_streams;
	
	uint32_t exit_code;
	pid_t pid_waiting_on;
} task_t;

task_t* task_create(void (*entry)(void));
task_t* task_create_user(void (*entry)(void));
task_t* task_create_frelf_user(const void* elf_data, size_t elf_size);
void task_switch_to(task_t* old, task_t* new);
void task_init_stack(task_t* t, void (*entry)(void));
void task_init_user_stack(task_t* t, void (*entry)(void), uint32_t user_esp);
void task_activate(task_t* task);
void task_destroy(task_t* task);
int task_build_user_argv_stack(
	uint32_t stack_phys,
	uint32_t stack_virt_top,
    char** argv,
    int argc,
    size_t max_bytes,
    uint32_t* out_esp
); 
pid_t task_procreate(task_t* caller, const procreate_args_t* args);
int task_wait_pid(pid_t target, int* exit_code_out);
void task_wake_waiters(pid_t exited_pid);

#endif
