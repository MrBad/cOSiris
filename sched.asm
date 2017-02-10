EXTERN fork_inner, get_last_task
[GLOBAL fork]

fork:
	cli								; do not interrupt me
	call fork_inner

	mov [eax + 8], esp				; save child esp
	mov [eax + 12], ebp				; save child ebp
	mov [eax + 16], dword .child	; next jump to fork.child

	push eax						; save child task_t
	call get_last_task				; get last task in task_queue
	pop ebx							; get back the child
	mov [eax + 24], ebx				; link this new child to task_queue

.parent:
	mov eax, [ebx]					; move the pid of child into eax
	jmp .bye						; jump back with child pid

.child:
	xor eax, eax					; here first jumps the child when it's first scheduled by task_switch
									; return 0 to it
.bye:
	sti
	ret

EXTERN get_current_task, get_next_task, print_int, ps, print_current_task, task_switch_inner
[GLOBAL task_switch]
task_switch:

	call get_current_task			; if current_task not inited, just return
	cmp eax, 0
	je .bye
									; current_task was initialised by task_init
									; saving it's interrupted state
	mov [eax + 8], esp				; save current_task esp
	mov [eax + 12], ebp				; save current_task ebp
	mov [eax + 16], dword .bye		; next time, jump to .bye

	;call get_next_task				; get next task in queue
	call task_switch_inner			; call inner so we have mode control in C

	mov esp, [eax + 8]				; move it's saved esp to esp
	mov ebp, [eax + 12]				; ebp
	mov ebx, [eax + 20]				; it's cloned directory
	mov cr3, ebx					; change page directory
	sti
	mov ecx, [eax+16]
	mov eax, 0x20
	out 0x20, al
	jmp ecx							; jump to it's saved eip -> probably task_switch.bye or fork.child
.bye:
	mov eax, 0x20
	out 0x20, al
	ret


GLOBAL get_flags
get_flags:
	pushfd
	pop eax
	ret
