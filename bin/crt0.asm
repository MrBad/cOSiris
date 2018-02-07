extern _ini, main, exit

section .text
global start
start:
    call _ini
    call main
    push eax
    call exit
    jmp $
