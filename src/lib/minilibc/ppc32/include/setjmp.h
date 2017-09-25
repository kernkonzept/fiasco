#ifndef _SETJMP_H
#define _SETJMP_H

#include <cdefs.h>

#define __JMP_BUF_SP            8
#ifndef __ASSEMBLER__
typedef int __jmp_buf[22];
#endif

#ifndef __ASSEMBLER__

typedef struct __jmp_buf_struct {	
  __jmp_buf __jmpbuf;		
} jmp_buf[1];


__BEGIN_DECLS

void longjmp (jmp_buf __env, int __val)
     __attribute__ ((__noreturn__));

int setjmp(jmp_buf env);

__END_DECLS


#endif /* ASSEMBLER */

#endif
