.global enter_usermode_trampoline
enter_usermode_trampoline:
	popl %eax        # entry
	popl %ecx        # user_esp

	movw $0x23, %dx
	movw %dx, %ds
	movw %dx, %es
	movw %dx, %fs
	movw %dx, %gs

	pushl $0x23       # SS
	pushl %ecx        # ESP
	pushf
	orl $0x200, (%esp) # force IF=1
	pushl $0x1b       # CS
	pushl %eax        # EIP
	iret