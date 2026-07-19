.section .text
.global panuti_syscall

# int32_t panuti_syscall(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
# cdecl stack layout on entry:
#   [esp+4]  = num
#   [esp+8]  = a1
#   [esp+12] = a2
#   [esp+16] = a3
#   [esp+20] = a4
panuti_syscall:
	push %ebp
	push %ebx
	push %esi
	push %edi
	# four pushes above shift args by 16 bytes: num is now at [esp+20], etc.

	movl 20(%esp), %eax   # num
	movl 24(%esp), %ebx   # a1
	movl 28(%esp), %esi   # a2
	movl 32(%esp), %edi   # a3
	movl 36(%esp), %ebp   # a4

	movl %esp, %ecx       # ECX = user ESP (hardware requirement for sysexit)
	movl $1f, %edx        # EDX = return address (hardware requirement)
	sysenter
1:
	# execution resumes here after sysexit -- EAX holds the return value

	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	ret