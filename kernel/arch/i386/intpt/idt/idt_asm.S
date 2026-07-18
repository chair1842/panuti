/* macro for exceptions without an error code */
.macro ISR_NOERR n
.global isr\n
isr\n:
	push $0
	push $\n
	jmp isr_common
.endm

/* macro for exceptions with an error code */
.macro ISR_ERR n
.global isr\n
isr\n:
	push $\n
	jmp isr_common
.endm

/* macro for IRQs */
.macro IRQ n, vec
.global irq\n
irq\n:
	push $0
	push $\vec
	jmp isr_common
.endm

/* exceptions */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

/* IRQs */
IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

isr_common:
	pusha
	push %ds
	push %es
	push %fs
	push %gs

	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es

	push %esp
	call isr_handler
	add $4, %esp

	pop %gs
	pop %fs
	pop %es
	pop %ds
	popa
	add $8, %esp
	iret

.global idt_load
idt_load:
    mov 4(%esp), %eax
    lidt (%eax)
    ret
