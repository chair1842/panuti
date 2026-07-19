.section .text
.global _start
.extern main

_start:
	call main
	# main's return value is in EAX -- pass it straight through as the exit code
	movl %eax, %ebx
	movl $1, %eax    # SYSHANDLER_EXIT
	xorl %ecx, %ecx
	movl $1f, %edx
	sysenter
1:
	# should never reach here -- exit doesn't return -- but just in case:
	jmp 1b