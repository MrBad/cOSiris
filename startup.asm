[BITS 32]

	MULTIBOOT_PAGE_ALIGN	equ 1<<0		; page align
	MULTIBOOT_MEMORY_INFO	equ 1<<1		; grub, paseaza mem info
	MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002	; grub, magic - one bad boot?
	MULTIBOOT_HEADER_FLAGS	equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO	; grub, flags
	CHECKSUM equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)				; grub, checksum



	GLOBAL start						; punct de intrare, unde va sari dupa ce grub incarca tot
	EXTERN main							; c main file
	EXTERN code, bss, end, data			; pasate de linker, vezi lscript.txt
	; functii scrise in asm, apelabile din c
	STACKSIZE equ 0x10000				; 64k stack size


[SECTION .text]

	align 4

	dd MULTIBOOT_HEADER_MAGIC
	dd MULTIBOOT_HEADER_FLAGS
	dd CHECKSUM
	; multiboot header, pt grub, tre sa fie in primii 8k


align 8
start:
	mov esp, stack+STACKSIZE
	mov ebp, esp
	push esp
	push STACKSIZE
	push ebx	; ptr struct multiboot_header
	push eax	; magic
	call main
.h:
	hlt
	jmp .h



; functii apelate din C
GLOBAL get_esp
get_esp:
	mov eax, esp
	ret
GLOBAL get_eip
get_eip:
	pop eax
	jmp eax

GLOBAL get_ebp
get_ebp:
	mov eax, ebp
	ret

;GLOBAL outb
;outb:
;	push ebp
;	mov ebp, esp
;	mov edx, [ebp+8]	; first arg - port
;	mov eax, [ebp+12]	; second arg - value
;	out dx, al
;	pop ebp
;	ret

GLOBAL bochs_magic_break
bochs_magic_break:
    xchg bx, bx
    ret

GLOBAL get_kernel_info
get_kernel_info:
	push ebp
	mov ebp, esp
	mov edx, end
	sub edx, code
	mov eax, [ebp+8];
	mov dword [eax], code
	mov dword [eax+4], start
	mov dword [eax+8], data
	mov dword [eax+12], bss
	mov dword [eax+16], end
	mov dword [eax+20], edx
	pop ebp
	ret

GLOBAL halt
halt:
	cli;
	hlt;


GLOBAL gdt_flush
EXTERN gdt_p
gdt_flush:
	lgdt[gdt_p]		;incarca tabela globala de descriptori (GDT) cu pointerul special gdt_p, definit in gdt.h
	mov ax, 0x10		;0x10 e offsetul catre segmentul de date // la 0x00 e null descriptor, la 0x08 e code, 0x10 data
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	jmp 0x08:flush_cs	;segmentul de data e mai special, nu poate fi incarcat direct, facem un far jump pentru a-l seta
flush_cs:
	ret


GLOBAL idt_load
EXTERN idt_p
idt_load:
	lidt [idt_p]
	ret

;; isrs ;;
GLOBAL isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
GLOBAL isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
GLOBAL isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
GLOBAL isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31, isr128


isr0:
	cli
	push byte 0
	push byte 0
	jmp isr_common

isr1:
	cli
	push byte 0
	push byte 1
	jmp isr_common

isr2:
	cli
	push byte 0
	push byte 2
	jmp isr_common

isr3:
	cli
	push byte 0
	push byte 3
	jmp isr_common

isr4:
	cli
	push byte 0
	push byte 4
	jmp isr_common

isr5:
	cli
	push byte 0
	push byte 5
	jmp isr_common

isr6:
	cli
	push byte 0
	push byte 6
	jmp isr_common

isr7:
	cli
	push byte 0
	push byte 7
	jmp isr_common

isr8:
	cli
	push byte 8	; - already pushed
	jmp isr_common

isr9:
	cli
	push byte 0
	push byte 9
	jmp isr_common

isr10:
	cli
	push byte 10
	jmp isr_common

isr11:
	cli
	push byte 11
	jmp isr_common

isr12:
	cli
	push byte 12
	jmp isr_common

isr13:
	cli
	push byte 13
	jmp isr_common

isr14:
	cli
	push byte 14
	jmp isr_common

isr15:
	cli
	push byte 0
	push byte 15
	jmp isr_common

isr16:
	cli
	push byte 0
	push byte 16
	jmp isr_common

isr17:
	cli
	push byte 0
	push byte 17
	jmp isr_common

isr18:
	cli
	push byte 0
	push byte 18
	jmp isr_common

isr19:
	cli
	push byte 0
	push byte 19
	jmp isr_common

isr20:
	cli
	push byte 0
	push byte 20
	jmp isr_common

isr21:
	cli
	push byte 0
	push byte 21
	jmp isr_common

isr22:
	cli
	push byte 0
	push byte 22
	jmp isr_common

isr23:
	cli
	push byte 0
	push byte 23
	jmp isr_common

isr24:
	cli
	push byte 0
	push byte 24
	jmp isr_common

isr25:
	cli
	push byte 0
	push byte 25
	jmp isr_common

isr26:
	cli
	push byte 0
	push byte 26
	jmp isr_common

isr27:
	cli
	push byte 0
	push byte 27
	jmp isr_common

isr28:
	cli
	push byte 0
	push byte 28
	jmp isr_common

isr29:
	cli
	push byte 0
	push byte 29
	jmp isr_common

isr30:
	cli
	push byte 0
	push byte 30
	jmp isr_common

isr31:
	cli
	push byte 0
	push byte 31
	jmp isr_common

isr128:
	cli
	push byte 0
	push 128
	jmp isr_common

extern isr_handler
isr_common:
	pusha
	push ds
	push es
	push fs
	push gs
	mov ax, 0x10 ; data segment
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov eax, esp
	push eax	 ; push us to stack
	call isr_handler
;	call eax
	pop eax
	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8	; cleanup pushed error codes and pushed isr numbers
	iret
;; end isrs ;;


;; irqs ;;
GLOBAL irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
GLOBAL irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

irq0:
	cli
	push byte 0
	push byte 32
	jmp irq_common

irq1:
	cli
	push byte 0
	push byte 33
	jmp irq_common

irq2:
	cli
	push byte 0
	push byte 34
	jmp irq_common

irq3:
	cli
	push byte 0
	push byte 35
	jmp irq_common

irq4:
	cli
	push byte 0
	push byte 36
	jmp irq_common

irq5:
	cli
	push byte 0
	push byte 37
	jmp irq_common

irq6:
	cli
	push byte 0
	push byte 38
	jmp irq_common

irq7:
	cli
	push byte 0
	push byte 39
	jmp irq_common

irq8:
	cli
	push byte 0
	push byte 40
	jmp irq_common

irq9:
	cli
	push byte 0
	push byte 41
	jmp irq_common

irq10:
	cli
	push byte 0
	push byte 42
	jmp irq_common

irq11:
	cli
	push byte 0
	push byte 43
	jmp irq_common

irq12:
	cli
	push byte 0
	push byte 44
	jmp irq_common

irq13:
	cli
	push byte 0
	push byte 45
	jmp irq_common

irq14:
	cli
	push byte 0
	push byte 46
	jmp irq_common

irq15:
	cli
	push byte 0
	push byte 47
	jmp irq_common


EXTERN irq_handler
irq_common:
	pusha
	push ds
	push es
	push fs
	push gs
	mov ax, 0x10 ; data segment
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov eax, esp
	push eax
	mov eax, irq_handler
	call eax
	pop eax
	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 2 * 4
	iret

;; end irqs ;;


;; activeaza paginarea ;;
;EXTERN page_directory
;GLOBAL activate_paging
;activate_paging:
;	mov eax, [page_directory]
;	mov cr3, eax
;	mov eax, cr0
;	or eax, 0x80000000		; set PG in cr0
;	mov cr0, eax
;	ret


global bochs_break
bochs_break:
    xchg bx, bx

GLOBAL switch_pd
switch_pd:
	push ebp
	mov ebp, esp
	mov eax, [ebp+8]
	mov cr3, eax
	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax
	pop ebp
	ret

GLOBAL flush_tlb
flush_tlb:
	mov eax, cr3
	mov cr3, eax
	ret

;flush_tlb:
;	push ebp
;	mov ebp, esp
;
 ;   mov eax, cr0
  ;  or eax, 0x80000000
   ; mov cr0, eax
;
;	pop ebp
;	ret

[SECTION .bss]
align 4096
stack:
	resb STACKSIZE                     ; reserve 16k stack on a doubleword boundary

[SECTION .data]
	stack_ptr		dd	0
	stack_size		dd	0
