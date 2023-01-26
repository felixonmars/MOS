// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mos_global.h"

should_inline long platform_syscall0(long number)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number));
    return result;
}

should_inline long platform_syscall1(long number, long arg1)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1));
    return result;
}

should_inline long platform_syscall2(long number, long arg1, long arg2)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2));
    return result;
}

should_inline long platform_syscall3(long number, long arg1, long arg2, long arg3)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3));
    return result;
}

should_inline long platform_syscall4(long number, long arg1, long arg2, long arg3, long arg4)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return result;
}

should_inline long platform_syscall5(long number, long arg1, long arg2, long arg3, long arg4, long arg5)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5));
    return result;
}

should_inline long platform_syscall6(long number, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6)
{
    long result = 0;
    __asm__ volatile("push %%rbx\n" // callee-saved
                     "push %%rdi\n" // callee-saved
                     "push %%rsi\n" // callee-saved
                     "push %%rbp\n" // callee-saved

                     "mov %1, %%rax\n"
                     "mov %2, %%rbx\n"
                     "mov %3, %%rcx\n"
                     "mov %4, %%rdx\n"
                     "mov %5, %%rsi\n"
                     "mov %6, %%rdi\n"
                     "mov %7, %%rbp\n"

                     "int $0x88\n"

                     "pop %%rbp\n"
                     "pop %%rsi\n"
                     "pop %%rdi\n"
                     "pop %%rbx\n"
                     : "=a"(result)
                     : "m"(number), "m"(arg1), "m"(arg2), "m"(arg3), "m"(arg4), "m"(arg5), "m"(arg6));
    return result;
}
