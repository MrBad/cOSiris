EXTERN task_new, clone_directory, get_last_task, print_int
[GLOBAL fork]

fork:
	;cli								; do not interrupt me
	call task_new					; get a new child (task_t)
	mov [eax + 4], esp				; save child esp
	mov [eax + 8], ebp				; save child ebp
	mov [eax + 12], dword .child	; next jump to fork.child

	push eax						; save current task_t
	call clone_directory			; clone the current address space and get a new directory
	mov ebx, eax					; save it to ebx
	pop eax							; get back the child
	mov [eax + 16], ebx				; save the cloned directory into the task_t->page_directory

	push eax						; save child task_t
	call get_last_task				; get last task in task_queue
	pop ebx							; get back the child
	mov [eax + 20], ebx				; link this new child to task_queue

.parent:
	mov eax, [ebx]					; move the pid of child into eax
	jmp .bye						; jump back with child pid

.child:
	xor eax, eax					; here first jumps the child when it's first scheduled by task_switch
									; return 0 to it
.bye:
	;sti
	ret

EXTERN get_current_task, get_next_task, print_int, ps, print_current_task
[GLOBAL task_switch]
task_switch:

	call get_current_task			; if current_task not inited, just return
	cmp eax, 0
	je .bye
									; current_task was initialised by task_init
									; saving it's interrupted state
	mov [eax + 4], esp				; save current_task esp
	mov [eax + 8], ebp				; save current_task ebp
	mov [eax + 12], dword .bye		; next time, jump to .bye

	call get_next_task				; get next task in queue

	cli
;	push eax
;	call print_int
;	pop eax

	mov esp, [eax + 4]				; move it's saved esp to esp
	mov ebp, [eax + 8]				; ebp
	mov ebx, [eax + 16]				; it's cloned directory
	mov cr3, ebx					; change page directory
	sti
	jmp [eax + 12]					; jump tp it's saved eip -> probably task_switch.bye or fork.child

.bye:
	ret


GLOBAL get_flags
get_flags:
	pushfd
	pop eax
	ret
