#ifndef _SETJMP_H
#define _SETJMP_H

#include <cdefs.h>


#ifndef __ASSEMBLER__
typedef int __jmp_buf[6];
#endif
# define JB_BX	0
# define JB_SI	1
# define JB_DI	2
# define JB_BP	3
# define JB_SP	4
# define JB_PC	5
# define JB_SIZE 24

#ifndef __ASSEMBLER__

typedef struct __jmp_buf_struct
{
  __jmp_buf __jmpbuf;
} jmp_buf[1];

__BEGIN_DECLS

void longjmp (jmp_buf __env, int __val)
     __attribute__ ((__noreturn__,__regparm__(3)));
int  setjmp(jmp_buf env) __attribute__((__regparm__(3)));

__END_DECLS

#endif

#endif
