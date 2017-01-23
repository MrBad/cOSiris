[BITS 32]
global _syscall0, _syscall1, _syscall2, _syscall3, _syscall4, _syscall5
_syscall0:
	mov eax, [esp + 4]
	int 0x80
	ret

_syscall1:
	push ebx
	mov eax, [esp + 4 + 4]
	mov ebx, [esp + 4 + 8]
	int 0x80
	pop ebx
	ret

_syscall2:
	push ebx
	push ecx

	mov eax, [esp + 8 + 4]
	mov ebx, [esp + 8 + 8]
	mov ecx, [esp + 8 + 12]
	int 0x80

	pop ecx
	pop ebx
	ret

_syscall3:
	push ebx
	push ecx
	push edx

	mov eax, [esp + 12 + 4]
	mov ebx, [esp + 12 + 8]
	mov ecx, [esp + 12 + 12]
	mov edx, [esp + 12 + 16]
	int 0x80

	pop edx
	pop ecx
	pop ebx
	ret

_syscall4:
	push ebx
	push ecx
	push edx
	push esi

	mov eax, [esp + 16 + 4]
	mov ebx, [esp + 16 + 8]
	mov ecx, [esp + 16 + 12]
	mov edx, [esp + 16 + 16]
	mov esi, [esp + 16 + 20]
	int 0x80

	pop esi
	pop edx
	pop ecx
	pop ebx

	ret

_syscall5:
	push ebx
	push ecx
	push edx
	push esi
	push edi

	mov eax, [esp + 20 + 4]
	mov ebx, [esp + 20 + 8]
	mov ecx, [esp + 20 + 12]
	mov edx, [esp + 20 + 16]
	mov esi, [esp + 20 + 20]
	mov edi, [esp + 20 + 24]

	pop edi
	pop esi
	pop edx
	pop ecx
	pop ebx

	int 0x80
	ret
