EXTERN task_new, clone_directory, get_last_task, print_int, getpid
[GLOBAL fork]

fork:
	cli								; do not interrupt me
	call getpid
	push eax						; save current pid
	call task_new					; get a new child (task_t)
	pop ecx
	mov [eax + 4], ecx				; save ppid
	mov [eax + 8], esp				; save child esp
	mov [eax + 12], ebp				; save child ebp
	mov [eax + 16], dword .child	; next jump to fork.child

	push eax						; save current task_t
	call clone_directory			; clone the current address space and get a new directory
	mov ebx, eax					; save it to ebx
	pop eax							; get back the child
	mov [eax + 20], ebx				; save the cloned directory into the task_t->page_directory

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
	jmp [eax + 16]					; jump tp it's saved eip -> probably task_switch.bye or fork.child

.bye:
	ret


GLOBAL get_flags
get_flags:
	pushfd
	pop eax
	ret
