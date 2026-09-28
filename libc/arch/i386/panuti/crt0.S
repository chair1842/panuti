/* SPDX-License-Identifier: BSD-3-Clause */

.section .text
.global _start
.extern main

# GCC emits _init/_fini for every program on this target: they call frame_dummy
# and walk the .ctors/.dtors tables that panuti.ld terminates with LONG(-1).
# Referenced weakly so that crt0 still links and runs if that ever stops being
# true -- an unresolved weak symbol reads as 0 and the call is skipped.
.weak _init
.weak _fini

_start:
	# stack layout on entry (written by build_user_argv_stack in the
	# kernel before this task was scheduled):
	#   esp+0            : argc
	#   esp+4            : argv[0] (pointer to string)
	#   esp+4+4*1        : argv[1]
	#   ...
	#   esp+4+4*argc     : NULL (argv[] terminator)
	#   (string bytes follow, referenced by the argv pointers above)

	# run ELF constructors (__attribute__((constructor))) before main. Done
	# before argc/argv are loaded, since _init may clobber any register.
	movl $_init, %ecx
	testl %ecx, %ecx
	je 3f
	call *%ecx
3:

	movl (%esp), %eax    # argc
	leal 4(%esp), %ebx   # &argv[0]

	pushl %ebx           # argv, second arg to main (pushed first, cdecl is right-to-left)
	pushl %eax           # argc, first arg to main

	call main
	addl $8, %esp        # pop the two pushed args, cdecl caller cleans up

	# main's return value is in EAX -- pass it straight through as the exit code
	movl %eax, %ebx

	# run ELF destructors (__attribute__((destructor))) before exiting. %ebx
	# already carries the exit code, so save it across the call.
	pushl %ebx
	movl $_fini, %ecx
	testl %ecx, %ecx
	je 4f
	call *%ecx
4:
	popl %ebx

	movl $1, %eax    # SYSHANDLER_EXIT
	xorl %ecx, %ecx
	movl $1f, %edx
	sysenter
1:
	# should never reach here -- exit doesn't return -- but just in case:
	jmp 1b
