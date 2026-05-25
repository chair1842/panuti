sysentry_handler:
	push %ecx
	push %edx

	sti

	push %ebx # arg4
	push %esi # arg3
	push %edi # arg2
	push %ebp # arg1
	push %eax # syscall number

	call syscall_handler

	add $20, %esp # discard frame

	cli

	pop %edx
	pop %ecx

	sysexit