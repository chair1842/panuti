# void task_switch_to(task_t* old, task_t* new)
.global task_switch_to
task_switch_to:
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	pushfl                # EFLAGS is part of the context; without saving it,
	                      # a task switched away from inside an IRQ handler
	                      # would resume with IF=0 (interrupt gates clear IF,
	                      # and iret only restores it for the task that was
	                      # interrupted, not the one scheduled in).

	movl 24(%esp), %eax	# old task ptr
	movl %esp, (%eax) # old->esp = current esp

	movl 28(%esp), %eax	# new task ptr
	movl 12(%eax), %eax	# i386 address-space handle (physical page directory)
	movl %eax, %cr3

	movl 28(%esp), %eax	# new task ptr
	movl (%eax), %esp # esp = new->esp

	popfl                 # restore incoming task's EFLAGS (incl. IF)
	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	ret
