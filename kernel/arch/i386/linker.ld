ENTRY(_start)

/* The kernel virtual base address. 3GB mark. */
KERNEL_VIRTUAL_OFFSET = 0xC0000000;

SECTIONS
{
    /* The kernel physically loads at 1MB but is linked at 3GB + 1MB */
    . = 0xC0100000;

    /* First put the multiboot header, as it is required to be put very early
       in the image or the bootloader won't recognize the file format.
       Next we'll put the .text section. */
    .text BLOCK(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_OFFSET) ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Read-only data. */
    .rodata BLOCK(4K) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_OFFSET) ALIGN(4K)
    {
        *(.rodata)
    }

    /* Read-write data (initialized) */
    .data BLOCK(4K) : AT(ADDR(.data) - KERNEL_VIRTUAL_OFFSET) ALIGN(4K)
    {
        *(.data)
    }

    /* Read-write data (uninitialized) and stack */
    .bss BLOCK(4K) : AT(ADDR(.bss) - KERNEL_VIRTUAL_OFFSET) ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}