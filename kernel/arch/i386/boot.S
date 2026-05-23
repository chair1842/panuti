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
.global boot_page_dir
boot_page_dir:
.skip 4096

.align 4096
.global boot_page_table
boot_page_table:
.skip 4096

.section .text
.global _start
.type _start, @function
_start:
	/* copy multiboot info pointer from ebx to esi for safe-keeping*/
	movl %ebx, %esi

	/* zero out page directory */
	movl $PHYS(boot_page_dir), %edi
	movl $1024, %ecx
	xorl %eax, %eax
	rep stosl

	/* zero out page table */
	movl $PHYS(boot_page_table), %edi
	movl $1024, %ecx
	xorl %eax, %eax
	rep stosl

	/* fill boot_page_table: map 4MB starting at physical 0 */
	movl $PHYS(boot_page_table), %edi
	movl $0x00000003, %eax  /* physical 0, present+writable */
	movl $1024, %ecx
.fill_table:
	movl %eax, (%edi)
	addl $4096, %eax        /* next 4KB page */
	addl $4, %edi           /* next entry */
	loop .fill_table

	/* point entry 0 (identity map) and entry 768 (upper half) at boot_page_table */
	movl $PHYS(boot_page_table), %eax
	orl $0x03, %eax
	movl %eax, PHYS(boot_page_dir)
	movl %eax, PHYS(boot_page_dir) + 768 * 4

	/* load cr3 */
	movl $PHYS(boot_page_dir), %eax
	movl %eax, %cr3

	/* enable paging */
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0

	/* jump to high memory */
	lea high_mem, %eax
	jmp *%eax

high_mem:
	/* now in high memory, safe to use virtual addresses */
	movl $stack_top, %esp

	/* initialize GDT */
	call gdt_init

	call _init

	/* initialize the PMM */
	push %esi
	call pmm_init
	add $4, %esp

	call pic_init
	call idt_init

	call vmm_init
	call vmalloc_init

	call kearly
	call kernel_main

	cli
1:  hlt
	jmp 1b
.size _start, . - _start