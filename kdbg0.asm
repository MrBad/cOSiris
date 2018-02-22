; Sets kdbg stack to a safe place, because we can do there context switching.
; Call the handler
EXTERN kdbg_handler, kdbg_stack_hi
GLOBAL kdbg_handler_enter
kdbg_handler_enter:
    push ebp
    mov ebp, esp
    mov esp, [kdbg_stack_hi]
    call kdbg_handler
    mov esp, ebp
    pop ebp
    ret

