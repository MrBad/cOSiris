global switch_to_user_mode_asm

; There is no simple way to switch to ring 3 from ring 0 on i386.
; You can of course return from ring 0 to ring 3.
; So we will simulate a calculated interrupt return, like it was from our
; user program
switch_to_user_mode_asm:
    cli

    mov ebx, [esp + 4]      ; save address to jump to
    mov ecx, [esp + 8]      ; save user ring 3 stack addr

    mov ax, 0x23            ; user mode data segment (gdt entry 4) with 
    mov ds, ax              ; bottom 2 bits set for ring 3
    mov es, ax
    mov fs, ax
    mov gs, ax
    push 0x23               ; push ring 3 'ss'
    push ecx                ; push new stack - 'esp'
    pushf                   ; push flags to stack
    or dword [esp], 0x200   ; patch pushed flags, sets Interrupt Flag to 1
    push 0x1b               ; push user mode code seg with bottom 2 bits set
    push ebx                ; ebx is the address where to "return" from int(IP)
    iret                    ; return from 'interrupt' - aka jump in ring 3,
                            ; where our program is loaded
    
    ret                     ; this should never return, but...

GLOBAL tss_flush
tss_flush:
    mov ax, 0x2B    ; Load the index of our TSS structure - The index is
                    ; 0x28, as it is the 5th entry in gdt and each is 8 bytes
                    ; long, but we set the bottom two bits (making 0x2B)
                    ; so that it has an RPL of 3, not zero.
    ltr ax          ; Load 0x2B into the task state register.
    ret

GLOBAL call_sys
call_sys:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 28]
    push eax
    mov eax, [ebp + 24]
    push eax
    mov eax, [ebp + 20]
    push eax
    mov eax, [ebp + 16]
    push eax
    mov eax, [ebp + 12]
    push eax
    mov eax, [ebp + 8]
    call eax
    add esp, 20

    pop ebp
    ret
