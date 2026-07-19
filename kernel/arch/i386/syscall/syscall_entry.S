.section .text
.global sysenter_entry
.extern syscall_dispatch

# EAX = syscall no.
# EBX/ESI/EDI/EBP = args 1-4
# ECX = user ESP, EDX = user return EIP
sysenter_entry:
	push %edx          # save user return EIP
	push %ecx          # save user ESP

	# DS/ES/FS/GS still hold user selectors on entry, sysenter doesn't touch them.
	# Reload kernel data segment before running any kernel C code.
	pushl $0x10
	pop %ds
	pushl $0x10
	pop %es
	pushl $0x10
	pop %fs
	pushl $0x10
	pop %gs

	# cdecl call: syscall_dispatch(num, a1, a2, a3, a4), push right-to-left
	push %ebp
	push %edi
	push %esi
	push %ebx
	push %eax
	call syscall_dispatch
	add $20, %esp
	# reclaim the 5 pushed args (5*4 bytes)
	# EAX now holds the return value

	# Restore user data segments before dropping back to ring 3
	pushl $0x23
	pop %ds
	pushl $0x23
	pop %es
	pushl $0x23
	pop %fs
	pushl $0x23
	pop %gs

	pop %ecx
	pop %edx
	sysexit