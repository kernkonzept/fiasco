/*
 * (c) ...
 *
 * Author: alexander.warg@kernkonzept.com
 */
#pragma once

#include <cdefs.h>

typedef long int jmp_buf[13];

__BEGIN_DECLS

void longjmp (jmp_buf __env, int __val)
     __attribute__ ((__noreturn__));

int setjmp(jmp_buf env);

__END_DECLS
