[BITS 32]
    EXTERN start, code, bss, end, data	; passed in by linker - ldscript.ld
    EXTERN stack_ptr, stack_size        ; saved in startup.asm
    STACKSIZE equ 0x4000			    ; stack size

[SECTION .text]

GLOBAL get_esp
    get_esp:
    mov eax, esp
    add eax, 4
    ret

GLOBAL get_ebp
get_ebp:
    mov eax, ebp
    ret

GLOBAL get_eip
get_eip:
    pop eax
    jmp eax

GLOBAL get_fault_addr
get_fault_addr:
    mov eax, cr2
    ret

GLOBAL halt
halt:
    cli
    hlt
    jmp halt;

GLOBAL cli
cli:
    cli
    ret

GLOBAL sti
sti:
    sti
    ret

GLOBAL nop
nop:
    nop
    ret

GLOBAL outb
outb:
    push ebp
    mov ebp, esp
    mov dx, [ebp+8]	; first arg - port
    mov al, [ebp+12]	; second arg - value
    out dx, al
    pop ebp
    ret

GLOBAL outw
outw:
    push ebp
    mov ebp, esp
    mov dx, [ebp+8]
    mov ax, [ebp+12]
    out dx, ax
    pop ebp
    ret

GLOBAL inb
inb:
    push ebp
    mov ebp, esp
    mov dx, [ebp + 8]
    in al, dx
    pop ebp
    ret

GLOBAL inw
inw:
    push ebp
    mov ebp, esp
    mov dx, [ebp + 8]
    in ax, dx
    pop ebp
    ret

GLOBAL get_kinfo
get_kinfo:
    push ebp
    mov ebp, esp
    mov edx, end
    sub edx, code
    mov eax, [ebp+8];
    mov dword [eax], code
    mov dword [eax+4], start
    mov dword [eax+8], data
    mov dword [eax+12], bss
    mov dword [eax+16], end     ; kernel end
    mov dword [eax+20], edx     ; kernel size(end - code)
    mov edx, [stack_ptr]
    add edx, [stack_size]
    mov dword [eax+24], edx
    mov dword [eax+28], STACKSIZE
    pop ebp
    ret

GLOBAL gdt_flush
EXTERN gdt_p
gdt_flush:
    lgdt[gdt_p]	;load global descriptor table(GDT) with gdt_p, see gdt.h
    mov ax, 0x10 ;0x10 is the offset to data segment; see gdt_init()
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:flush_cs   ; Code segment cannot be loaded directly, we need to
                        ; do a far jump in order to set it up
flush_cs:
    ret

GLOBAL idt_load
EXTERN idt_p
idt_load:
    lidt [idt_p]
    ret

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

GLOBAL invlpg
invlpg:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    invlpg [eax] 
    pop ebp
    ret

EXTERN kinfo
GLOBAL patch_stack
patch_stack:
    cli
    push ebp
    mov ebp, esp
    mov ecx, [kinfo + 28] ; size of stack
    mov edi, [ebp + 8]    ; KERNEL_STACK_HI
    sub edi, ecx          ; KERNEL_STACK_LOW
    shr ecx, 2            ; we read 4 bytes at a time
    mov esi, [stack_ptr]  ; save in esi current stack low
    
    mov edx, edi          ; save in edx the difference 
    sub edx, esi          ; between old stack and the new stack

copyb:
    lodsd                 ; load in eax from es:esi; esi+=4
    cmp eax, [stack_ptr]  ; is the value less than stack low
    jl store              ; no need to patch the value
    cmp eax, [kinfo + 24] ; is the value greater or eq stack hi
    jge store             ; no need to patch the value

    add eax, edx          ; eax is referring to an old stack value
                          ; we will add the offset to new stack

store:
    stosd                 ; store eax at es:edi, edi+=4
    loop copyb            ; decrement ecx

    mov eax, ebp
    add eax, edx
    mov ebp, eax
    
    mov eax, esp
    add eax, edx
    mov esp, eax

    pop ebp
    sti
    ret

global spin_lock
spin_lock:
    mov edx, [esp + 4]
    mov ecx, 1
    .retry:
    xor eax, eax
    lock cmpxchg [edx], cl
    jnz .retry
    ret

;; isrs ;;
GLOBAL isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
GLOBAL isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
GLOBAL isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
GLOBAL isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31, isr128

isr0:
    push byte 0
    push byte 0
    jmp isr_common

isr1:
    push byte 0
    push byte 1
    jmp isr_common

isr2:
    push byte 0
    push byte 2
    jmp isr_common

isr3:
    push byte 0
    push byte 3
    jmp isr_common

isr4:
    push byte 0
    push byte 4
    jmp isr_common

isr5:
    push byte 0
    push byte 5
    jmp isr_common

isr6:
    push byte 0
    push byte 6
    jmp isr_common

isr7:
    push byte 0
    push byte 7
    jmp isr_common

isr8:
    push byte 8
    jmp isr_common
    
isr9:
    push byte 0
    push byte 9
    jmp isr_common

isr10:
    push byte 10
    jmp isr_common

isr11:
    push byte 11
    jmp isr_common

isr12:
    push byte 12
    jmp isr_common
 
isr13:
    push byte 13
    jmp isr_common

isr14:
    push byte 14
    jmp isr_common

isr15:
    push byte 0
    push byte 15
    jmp isr_common

isr16:
    push byte 0
    push byte 16
    jmp isr_common

isr17:
    push byte 0
    push byte 17
    jmp isr_common

isr18:
    push byte 0
    push byte 18
    jmp isr_common

isr19:
    push byte 0
    push byte 19
    jmp isr_common

isr20:
    push byte 0
    push byte 20
    jmp isr_common

isr21:
    push byte 0
    push byte 21
    jmp isr_common

isr22:
    push byte 0
    push byte 22
    jmp isr_common

isr23:
    push byte 0
    push byte 23
    jmp isr_common

isr24:
    push byte 0
    push byte 24
    jmp isr_common

isr25:
    push byte 0
    push byte 25
    jmp isr_common

isr26:
    push byte 0
    push byte 26
    jmp isr_common

isr27:
    push byte 0
    push byte 27
    jmp isr_common

isr28:
    push byte 0
    push byte 28
    jmp isr_common

isr29:
    push byte 0
    push byte 29
    jmp isr_common

isr30:
    push byte 0
    push byte 30
    jmp isr_common

isr31:
    push byte 0
    push byte 31
    jmp isr_common

isr128:
    sti
    push byte 0
    push 128
    jmp isr_common

EXTERN isr_handler
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
    pop eax
    
    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8	; cleanup pushed error code and pushed isr number
    iret

;; end isrs ;;


;; irqs ;;

GLOBAL irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
GLOBAL irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

irq0:
    push byte 0
    push byte 32
    jmp irq_common

irq1:
    push byte 0
    push byte 33
    jmp irq_common

irq2:
    push byte 0
    push byte 34
    jmp irq_common

irq3:
    push byte 0
    push byte 35
    jmp irq_common

irq4:
    ;sti
    push byte 0
    push byte 36
    jmp irq_common

irq5:
    push byte 0
    push byte 37
    jmp irq_common

irq6:
    push byte 0
    push byte 38
    jmp irq_common

irq7:
    push byte 0
    push byte 39
    jmp irq_common

irq8:
    push byte 0
    push byte 40
    jmp irq_common

irq9:
    push byte 0
    push byte 41
    jmp irq_common

irq10:
    push byte 0
    push byte 42
    jmp irq_common

irq11:
    push byte 0
    push byte 43
    jmp irq_common

irq12:
    push byte 0
    push byte 44
    jmp irq_common

irq13:
    push byte 0
    push byte 45
    jmp irq_common

irq14:
    push byte 0
    push byte 46
    jmp irq_common

irq15:
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
