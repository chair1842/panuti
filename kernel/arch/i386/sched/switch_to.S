# void task_switch_to(task_t* old, task_t* new)
.global task_switch_to
task_switch_to:
	push %ebp
	push %ebx
	push %esi
	push %edi

	movl 20(%esp), %eax	# old task ptr
	movl %esp, (%eax) # old->esp = current esp

	movl 24(%esp), %eax	# new task ptr
	movl 12(%eax), %eax	# i386 address-space handle (physical page directory)
	movl %eax, %cr3

	movl 24(%esp), %eax	# new task ptr
	movl (%eax), %esp # esp = new->esp

	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	ret
