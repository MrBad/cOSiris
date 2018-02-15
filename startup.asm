[BITS 32]

	MULTIBOOT_PAGE_ALIGN	equ 1<<0		; page align
	MULTIBOOT_MEMORY_INFO	equ 1<<1		; pass me infos about memory
	MULTIBOOT_VBE_INFO      equ 1<<2		; pass me infos about video
	MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002	; multiboot, magic - one bad boot?
	MULTIBOOT_HEADER_FLAGS	equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
	CHECKSUM equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)	; checksum

	GLOBAL start                    ; kernel entry point
	GLOBAL stack_ptr, stack_size    ; save stack pointer and size
	EXTERN kmain                    ; main function to call
	EXTERN code, bss, end, data	    ; passed in by linker - ldscript.ld
	STACKSIZE equ 0x4000            ; stack size

[SECTION .text]
	; multiboot header
    align 4
	dd MULTIBOOT_HEADER_MAGIC
	dd MULTIBOOT_HEADER_FLAGS
	dd CHECKSUM

align 8
start:
    mov [stack_ptr], dword stack
    mov [stack_size], dword STACKSIZE
	mov esp, stack+STACKSIZE
	mov ebp, esp
	push ebx    ; push pointer to struct multiboot_header, passed by grub
	push eax    ; magic
	call kmain
.h:
	hlt
	jmp .h

[SECTION .bss]
align 4096
stack:
	resb STACKSIZE                     ; reserve 16k stack on a doubleword boundary

[SECTION .data]
	stack_ptr		dd	0
	stack_size		dd	0
