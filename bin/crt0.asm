extern main, exit

section .text
global start
start:
	call main
	push eax
	call exit
	jmp $
