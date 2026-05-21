.global gdt_load
gdt_load:
	mov 4(%esp), %eax
	lgdt (%eax)
	ret

# shwoosh, taken from osdev.wiki
.global gdt_reload_segments
gdt_reload_segments:
	ljmp $0x08, $.reload_cs
.reload_cs:
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	ret