#define KERNEL_VIRTUAL_OFFSET 0xC0000000
#define PHYS(x) ((x) - KERNEL_VIRTUAL_OFFSET)

.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.align 4096
boot_page_dir:
.skip 4096

.section .text
.global _start
.type _start, @function
_start:
	/* enable PSE (4MB pages) by setting bit 4 of cr4 */
	movl %cr4, %eax
	orl $0x10, %eax
	movl %eax, %cr4

	/* set up bootstrap page directory */
	movl $(0x00000000 | 0x83), PHYS(boot_page_dir)
	movl $(0x00000000 | 0x83), PHYS(boot_page_dir) + 768 * 4

	/* load physical address of page directory into cr3 */
	movl $PHYS(boot_page_dir), %eax
	movl %eax, %cr3

	/* enable paging by setting bit 31 of cr0 */
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0

	/* jump to high memory */
	lea high_mem, %eax
	jmp *%eax

high_mem:
	/* now in high memory, safe to use virtual addresses */
	movl $stack_top, %esp

	movl $0, boot_page_dir
	invlpg (0)

	call _init
	call kernel_main

	cli
1:  hlt
	jmp 1b
.size _start, . - _start