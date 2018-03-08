GLOBAL fork
fork:
    push ebp
    mov ebp, esp
    mov eax, 1
    int 0x80
    mov esp, ebp
    pop ebp
    ret
