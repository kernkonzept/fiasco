#include "asm_regs.h"
#include "asm_mips.h"

.text
.weak setjmp
.type setjmp,function
setjmp:
LEAF(__setjmp)
    ASM_S      ra, 0(a0)
    ASM_S      sp, (ASM_WORD_BYTES * 1)(a0)

    ASM_S      $16,(ASM_WORD_BYTES * 2)(a0)
    ASM_S      $17,(ASM_WORD_BYTES * 3)(a0)
    ASM_S      $18,(ASM_WORD_BYTES * 4)(a0)
    ASM_S      $19,(ASM_WORD_BYTES * 5)(a0)
    ASM_S      $20,(ASM_WORD_BYTES * 6)(a0)
    ASM_S      $21,(ASM_WORD_BYTES * 7)(a0)
    ASM_S      $22,(ASM_WORD_BYTES * 8)(a0)
    ASM_S      $23,(ASM_WORD_BYTES * 9)(a0)

    ASM_S      $30,(ASM_WORD_BYTES * 10)(a0)
    ASM_S      gp,(ASM_WORD_BYTES * 11)(a0)

    move    $2,$0
    j      $31

END(__setjmp)


.text
LEAF(longjmp)
    ASM_L      ra,  0(a0)
    ASM_L      sp,  (ASM_WORD_BYTES * 1)(a0)

    ASM_L      $16, (ASM_WORD_BYTES * 2)(a0)
    ASM_L      $17, (ASM_WORD_BYTES * 3)(a0)
    ASM_L      $18, (ASM_WORD_BYTES * 4)(a0)
    ASM_L      $19, (ASM_WORD_BYTES * 5)(a0)
    ASM_L      $20, (ASM_WORD_BYTES * 6)(a0)
    ASM_L      $21, (ASM_WORD_BYTES * 7)(a0)
    ASM_L      $22, (ASM_WORD_BYTES * 8)(a0)
    ASM_L      $23, (ASM_WORD_BYTES * 9)(a0)

    ASM_L      $30, (ASM_WORD_BYTES * 10)(a0)
    ASM_L      gp,  (ASM_WORD_BYTES * 11)(a0)

    bne $5,$0,1f
    li  $5,1
1:
    move    $2,$5
    ASM_L      $25, 0(a0)
    jr      $25
END(longjmp)
