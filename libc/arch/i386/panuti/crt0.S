/* SPDX-License-Identifier: BSD-3-Clause */

.section .text
.global _start
.extern main

_start:
	# stack layout on entry (written by build_user_argv_stack in the
	# kernel before this task was scheduled):
	#   esp+0            : argc
	#   esp+4            : argv[0] (pointer to string)
	#   esp+4+4*1        : argv[1]
	#   ...
	#   esp+4+4*argc     : NULL (argv[] terminator)
	#   (string bytes follow, referenced by the argv pointers above)

	movl (%esp), %eax    # argc
	leal 4(%esp), %ebx   # &argv[0]

	pushl %ebx           # argv, second arg to main (pushed first, cdecl is right-to-left)
	pushl %eax           # argc, first arg to main

	call main
	addl $8, %esp        # pop the two pushed args, cdecl caller cleans up

	# main's return value is in EAX -- pass it straight through as the exit code
	movl %eax, %ebx
	movl $1, %eax    # SYSHANDLER_EXIT
	xorl %ecx, %ecx
	movl $1f, %edx
	sysenter
1:
	# should never reach here -- exit doesn't return -- but just in case:
	jmp 1b