#pragma once

#include <cdefs.h>

typedef struct __jmp_buf_struct
  {
    /* Program counter.  */
    long __pc;
    /* Callee-saved registers. */
    long __regs[12];
    /* Stack pointer.  */
    long __sp;
  } jmp_buf[1];

__BEGIN_DECLS

void longjmp (jmp_buf __env, int __val)
     __attribute__ ((__noreturn__));

int setjmp(jmp_buf env);

__END_DECLS
