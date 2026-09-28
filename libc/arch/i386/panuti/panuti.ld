/* SPDX-License-Identifier: BSD-3-Clause */

ENTRY(_start)

PHDRS
{
	/* PF_R | PF_X, and PF_R | PF_W. The kernel's elf loader only honors PF_W
	   (there is no NX bit in 32-bit PTEs), so :text really does mean
	   "not writable" rather than "not writable and not executable". */
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}

SECTIONS
{
	. = 0x08048000;

	.text : ALIGN(4)
	{
		/* .text.* catches compiler-emitted variants (.text.startup holds the
		   optimized body of main), .stub catches linker-synthesized thunks */
		*(.text .text.* .gnu.linkonce.t.* .stub)
		*(.init)
		*(.fini)
	} :text

	.rodata : ALIGN(4)
	{
		/* .rodata.* catches .rodata.str1.1 / .str1.4 (string literals) and
		   .rodata.cst4 / .cst8 (constant pools) */
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	} :text

	/* kept (not discarded) so a future backtrace()/unwinder has something to
	   read; binutils has special handling for .eh_frame, so it gets its own
	   output section rather than being folded into .rodata */
	.eh_frame : ALIGN(4)
	{
		*(.eh_frame)
	} :text

	.eh_frame_hdr : ALIGN(4)
	{
		*(.eh_frame_hdr)
	} :text

	/* Mandatory, not cosmetic. elf_load_segments() allocates a fresh frame per
	   segment and maps it over the segment's pages, so two segments sharing a
	   page means the second mapping clobbers the first's bytes and leaves the
	   rest of that page holding whatever the new frame contained. Aligning the
	   boundary to a page keeps the two segments disjoint and lets us skip a
	   loader change entirely. */
	. = ALIGN(0x1000);

	.data : ALIGN(4)
	{
		*(.data .data.* .gnu.linkonce.d.*)
	} :data

	. = ALIGN(4);

	/* GCC for this target does not use .init_array -- it registers
	   constructors and destructors through the legacy .ctors/.dtors tables and
	   emits global _init/_fini that walk them; crt0.S calls those weakly.

	   These must stay SEPARATE output sections. __do_global_ctors_aux is
	   generated with addresses baked in relative to the table, so folding
	   .ctors/.dtors into .data relocates them against each other: the walk
	   then runs off the end of the ctor list, calls into the dtor list, and
	   eventually dereferences a null function pointer. LONG(-1) supplies the
	   -1 sentinel the walk terminates on. */
	.ctors : ALIGN(4)
	{
		*(.ctors)
		LONG(-1)
	} :data

	.dtors : ALIGN(4)
	{
		*(.dtors)
		LONG(-1)
	} :data

	/* NOBITS: contributes address space only, never file size */
	.bss : ALIGN(4)
	{
		*(COMMON)
		*(.bss .bss.*)
	} :data

	/DISCARD/ :
	{
		*(.comment)
		*(.note .note.*)
	}

	_end = .;

	/* first page boundary past the image, so a future userspace heap has a
	   deliberate base instead of relying on a page-table gap scan */
	. = ALIGN(0x1000);
	_heap_start = .;
}
