#pragma once

#include <cdefs.h>

typedef long int __jmp_buf[12];

typedef struct __jmp_buf_struct
{
  /* Program counter.  */
  long int __pc;

  /* Stack pointer.  */
  long int __sp;

  /* Callee-saved registers s0 through s7.  */
  long int __regs[8];

  /* The frame pointer.  */
  long int __fp;

  /* The global pointer.  */
  long int __gp;
} jmp_buf[1];

__BEGIN_DECLS

void longjmp (jmp_buf __env, int __val)
     __attribute__ ((__noreturn__));

int setjmp(jmp_buf env);

__END_DECLS
