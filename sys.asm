extern test_user_mode, print_int
global switch_to_user_mode_asm
switch_to_user_mode_asm:
        cli
        mov ax, 0x23
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        push 0x23					; ring 3 ss
        push 0xFFFF000				; ring 3 esp
        pushf						; save flags
        or dword [esp], 0x200		; enable interrupts after return
        push 0x1b                   ; user mode code segment
        ;push .sjmp					; eip where to return
        push 0x10000000
        iret
.sjmp
        jmp 0x10000000                  ; hardcoded user mode program
        ; call test_user_mode
        ret

GLOBAL tss_flush
tss_flush:
   mov ax, 0x2B      ; Load the index of our TSS structure - The index is
                     ; 0x28, as it is the 5th selector and each is 8 bytes
                     ; long, but we set the bottom two bits (making 0x2B)
                     ; so that it has an RPL of 3, not zero.
   ltr ax            ; Load 0x2B into the task state register.
   ret
