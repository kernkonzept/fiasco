// This function performs an unsigned division + modulo operation like
// __udivmod() with 64-bit operands on 32-bit hosts where
//
// - dividend and divisor reside in r0,r1 and r2,r3 (normal ARM32 calling
//   convention), and
// - quotient and remainder reside in r0,r1 and r2,r3 as well
//
// This ABI call cannot be achieved using generated C code.
// No, the compiler parameter -freg-struct-return doesn't help either. :-(

        .p2align 2
        .global __aeabi_uldivmod
        .type __aeabi_uldivmod, #function
__aeabi_uldivmod:
        push  {lr}
        sub   sp, sp, #12
        add   r12, sp, #4
        str   r12, [sp]
        // parameters for __udivmod4:
        //   r0,r1: div
        //   r2,r3: s
        //   sp[0]: &r => sp[4],sp[8]: r (remainder)
        bl    __udivmoddi4
        // __udivmod4 returned:
        //   quotient in r0,r1
        ldr   r2, [sp, #4]
        ldr   r3, [sp, #8]
        // remainder now in r2,r3
        add   sp, sp, #12
        pop   {pc}

// 32-bit unsigned division + modulo operation on 32-bit hosts lacking the
// `udiv` instruction.
        .p2align 2
        .global __aeabi_uidivmod
        .type __aeabi_uidivmod, #function
__aeabi_uidivmod:
        push  {lr}
        sub   sp, sp, #12
        mov   r2, r1
        mov   r1, r0
        mov   r0, sp
        // parameters for moddiv:
        //   r0: &result => sp[0]:quotient,sp[4]:remainder
        //   r1: div
        //   r2: s
        bl    umoddiv
        ldr   r0, [sp]
        ldr   r1, [sp, #4]
        add   sp, sp, #12
        pop   {pc}


// 32-bit unsigned division on 32-bit hosts lacking the `udiv` instruction.
        .p2align 2
        .global __aeabi_uidiv
        .type __aeabi_uidiv, #function
__aeabi_uidiv:
        push  {lr}
        sub   sp, sp, #12
        mov   r2, r1
        mov   r1, r0
        mov   r0, sp
        // parameters for moddiv:
        //   r0: &result => sp[0]:quotient,sp[4]:remainder
        //   r1: div
        //   r2: s
        bl   umoddiv
        ldr   r0, [sp]
        // remainder ignored
        add   sp, sp, #12
        pop   {pc}
