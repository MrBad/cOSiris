; TODO: can we simplify this?
EXTERN fork_inner, get_last_task
[GLOBAL fork]

fork:
    cli                             ; do not interrupt me
    call fork_inner

    mov [eax + 8], esp              ; save child esp
    mov [eax + 12], ebp             ; save child ebp
    mov [eax + 16], dword .child    ; next jump to fork.child

    push eax                        ; save child task_t
    call get_last_task              ; get last task in task_queue
    pop ebx                         ; get back the child
    mov [eax + 24], ebx             ; link this new child to task_queue

.parent:
    mov eax, [ebx]                  ; move the pid of child into eax
    jmp .bye                        ; jump back with child pid in ret

.child:
    xor eax, eax                    ; here jumps the child when it's first 
                                    ; scheduled by task_switch
                                    ; return 0 if child
.bye:
    sti
    ret

EXTERN current_task, task_switch_inner, process_signals
[GLOBAL task_switch]
task_switch:
    cli ; clear the interrupts, because function can be called from isr0x80
        ; with IFLAG On.
    mov eax, [current_task]           ; if current_task not inited, just return
    cmp eax, 0
    je .bye
    ; current_task was initialised by task_init
    ; saving it's interrupted state
    mov [eax + 8], esp              ; save current_task esp
    mov [eax + 12], ebp             ; save current_task ebp
    mov [eax + 16], dword .bye      ; next time, jump to .bye

    call task_switch_inner          ; call inner so we have mode control in C

    mov esp, [eax + 8]              ; move it's saved esp to esp
    mov ebp, [eax + 12]             ; ebp
    mov ebx, [eax + 20]             ; it's cloned directory
    mov cr3, ebx                    ; change page directory
    ;mov ecx, [eax+16]
    call process_signals            ; let's process the signals before we jump
    mov eax, [current_task]         ; read it again, we can be in other thask
    mov ecx, [eax + 16]              ; already
    jmp ecx                         ; jump to it's saved eip; 
                                    ; task_switch.bye or fork.child
.bye:
    ret

