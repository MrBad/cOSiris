GLOBAL switch_to_user_mode_asm
switch_to_user_mode_asm:
	cli
	mov ax, 0x23
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov eax, esp
	push 0x23
	push eax
	pushf
	pop eax
	or eax, 0x200
	push eax
	push 0x1b
	push .perf
	iret
.perf
	ret


GLOBAL tss_flush
tss_flush:
   mov ax, 0x2B      ; Load the index of our TSS structure - The index is
                     ; 0x28, as it is the 5th selector and each is 8 bytes
                     ; long, but we set the bottom two bits (making 0x2B)
                     ; so that it has an RPL of 3, not zero.
   ltr ax            ; Load 0x2B into the task state register.
   ret

GLOBAL blurp
blurp:
	mov ax, 0x10
	mov ds, ax
