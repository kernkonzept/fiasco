// This function performs a signed division + modulo operation like __udivmod()
// where
//
// - dividend and divisor reside in r0,r1 and r2,r3 (normal ARM32 calling
//   convention), and
// - quotient and remainder reside in r0,r1 and r2,r3 as well
//
// This ABI call cannot be achieved using generated C code.
// No, the compiler parameter -freg-struct-return doesn't help either. :-(

        .p2align 2
        .global __aeabi_uldivmod
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
