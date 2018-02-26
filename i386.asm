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

GLOBAL hlt
hlt:
    hlt
    ret
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
    mov esp, ebp
    pop ebp
    ret

GLOBAL outw
outw:
    push ebp
    mov ebp, esp
    mov dx, [ebp+8]
    mov ax, [ebp+12]
    out dx, ax
    mov esp, ebp
    pop ebp
    ret

GLOBAL inb
inb:
    push ebp
    mov ebp, esp
    mov dx, [ebp + 8]
    in al, dx
    mov esp, ebp
    pop ebp
    ret

GLOBAL inw
inw:
    push ebp
    mov ebp, esp
    mov dx, [ebp + 8]
    in ax, dx
    mov esp, ebp
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
    mov esp, ebp
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
    mov eax, [esp + 4]
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ret

EXTERN current_task
GLOBAL switch_context
switch_context:
    pop ecx     ; get return address
    mov eax, [current_task]
    mov esp, [eax + 0x108]
    mov ebp, [eax + 0x104]
    mov eax, [eax + 20]
    mov cr3, eax
    jmp ecx     ; We cannot use ret, because we are on different stack.

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
    mov esp, ebp
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

; Interrupts.
; We want a snapshot of registers as they were before the interrupt, so
; few patches need to be done. Also, we try to respect ABI calling convention
; so that's why the things are not so straight. Using full macro to avoid jumps,
; because for the reason i do not understand, gdb starts not to understand
; the frame (gives ??).

[EXTERN int_handler]
%macro INT_COMMON 0
    add esp, 28 ; go back
    push dword [ebp + 20]
    push dword [ebp + 16]
    push dword [ebp + 12]
    push dword [ebp + 8]
    push dword [ebp + 4]
    sub esp, 8 ; skip err code and int no
    pusha
    push ds
    push es
    push fs
    push gs
    mov eax, [ebp]
    mov [ebp - 52], eax ; get ebp as it was before interrupt
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp

    call int_fix_esp
    call int_handler
    call int_return
%endmacro

; We want the esp value before the interrupt, so we need a little patch
int_fix_esp:
    push ebp
    mov ebp, esp
    mov ebx, [ebp + 8]  ; points to iregs
    mov eax, [ebx + 28] ; get esp
    mov ecx, [ebx + 60] ; get cs to check for ring level
    and ecx, 0x03
    jnz .usr
    add eax, 44 ; patch for ring 0
    ;mov [ebx + 68], eax
    mov dword [ebx + 68], 0
    mov [ebx + 72], ss
    jmp .krnl
.usr:
    add eax, 52 ; in ring 3, we should add 2 more dwords, ss and user esp
.krnl:
    mov [ebx + 28], eax
    mov esp, ebp
    pop ebp
    ret

; Returns from an interrupt, and load registers from (iregs)
; void int_return(struct iregs *r)
[GLOBAL int_return]
int_return:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov ecx, [eax + 60] ; cs
    mov esp, [eax + 28] ; old esp
    and ecx, 0x03
    jz .krnl
    ; Construct return stack. Reusing stack frame from interrupt.
    ; If returning to ring3 we also must push ss:esp
    push dword [eax + 72] ; ss
    push dword [eax + 68] ; user esp
.krnl:
    push dword [eax + 64] ; eflags
    push dword [eax + 60] ; cs
    push dword [eax + 56] ; eip
    mov [eax + 28], esp   ; save new constructed return stack;
    mov esp, eax
    pop gs
    pop fs
    pop es
    pop ds
    pop edi
    pop esi
    pop ebp
    add esp, 4 ; skip esp, we will get it last
    pop ebx
    pop edx
    pop ecx
    pop eax
    mov esp, [esp - 20] ; get skipped esp last
    iret

%macro ISR_NO_ERR 1     ; macro with one parameter
[GLOBAL isr%1]          ; %1 access it's first parameter
isr%1:
    push ebp
    mov ebp, esp
    sub esp, 20 ; skip ss-eip (5 dwords)
    push 0
    push %1
    INT_COMMON
%endmacro

%macro ISR_ERR 1
[GLOBAL isr%1]
isr%1:
    xchg ebp, [esp]
    sub esp, 20
    push ebp
    lea ebp, [esp + 24]
    push %1
    INT_COMMON
%endmacro

%macro IRQ 1
[GLOBAL irq%1]
irq%1:
    push ebp
    mov ebp, esp
    sub esp, 20 ; skip ss-eip (5 dwords)
    push 0
    push %1 + 32
    INT_COMMON
%endmacro

ISR_NO_ERR 0
ISR_NO_ERR 1
ISR_NO_ERR 2
ISR_NO_ERR 3
ISR_NO_ERR 4
ISR_NO_ERR 5
ISR_NO_ERR 6
ISR_NO_ERR 7
ISR_ERR 8
ISR_NO_ERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NO_ERR 15
ISR_NO_ERR 16
ISR_NO_ERR 17
ISR_NO_ERR 18
ISR_NO_ERR 19
ISR_NO_ERR 20
ISR_NO_ERR 21
ISR_NO_ERR 22
ISR_NO_ERR 23
ISR_NO_ERR 24
ISR_NO_ERR 25
ISR_NO_ERR 26
ISR_NO_ERR 27
ISR_NO_ERR 28
ISR_NO_ERR 29
ISR_NO_ERR 30
ISR_NO_ERR 31
ISR_NO_ERR 128

IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

