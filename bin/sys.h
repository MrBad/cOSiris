#ifndef _SYS_H
#define _SYS_H

#include <types.h>

extern uint32_t _syscall0(uint32_t sys_num);
extern uint32_t _syscall1(uint32_t sys_num, uint32_t arg1);
extern uint32_t _syscall2(uint32_t sys_num, uint32_t arg1, uint32_t arg2);
extern uint32_t _syscall3(uint32_t sys_num, uint32_t arg1, uint32_t arg2, uint32_t arg3);
extern uint32_t _syscall4(uint32_t sys_num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
extern uint32_t _syscall5(uint32_t sys_num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

#endif
