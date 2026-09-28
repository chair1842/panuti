/* SPDX-License-Identifier: BSD-3-Clause */

ENTRY(_start)

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
	}

.rodata : ALIGN(4)
	{
		/* .rodata.* catches .rodata.str1.1 / .str1.4 (string literals) and
		   .rodata.cst4 / .cst8 (constant pools) */
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}

	/* kept (not discarded) so a future backtrace()/unwinder has something to
	   read; binutils has special handling for .eh_frame, so it gets its own
	   output section rather than being folded into .rodata */
.eh_frame : ALIGN(4)
	{
		*(.eh_frame)
	}

.eh_frame_hdr : ALIGN(4)
	{
		*(.eh_frame_hdr)
	}

	.data : ALIGN(4)
	{
		*(.data .data.* .gnu.linkonce.d.*)
	}

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
	}

	.dtors : ALIGN(4)
	{
		*(.dtors)
		LONG(-1)
	}

	/* NOBITS: contributes address space only, never file size */
	.bss : ALIGN(4)
	{
		*(COMMON)
		*(.bss .bss.*)
	}

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
