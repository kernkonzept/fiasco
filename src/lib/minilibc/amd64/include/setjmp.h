#ifndef _SETJMP_H
#define _SETJMP_H

#include <cdefs.h>


#ifndef __ASSEMBLER__
typedef long long unsigned __jmp_buf[12];
#endif
#define JB_BX	0
#define JB_BP	1
#define JB_R8	2
#define JB_R9	3
#define JB_R10	4
#define JB_R11	5
#define JB_R12	6
#define JB_R13	7
#define JB_R14	8
#define JB_R15	9
#define JB_SP	10
#define JB_PC	11
#define JB_SIZE 96

#ifndef __ASSEMBLER__

typedef struct __jmp_buf_struct
{
  __jmp_buf __jmpbuf;
} jmp_buf[1];

__BEGIN_DECLS

void longjmp (jmp_buf __env, int __val)
     __attribute__ ((__noreturn__));

int setjmp(jmp_buf env);

__END_DECLS

#endif

#endif
