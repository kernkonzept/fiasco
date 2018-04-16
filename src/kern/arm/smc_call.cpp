INTERFACE [arm && 32bit]:

#define FIASCO_ARM_SMC_CALL_ASM_OPERANDS \
    : "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3) \
    : "0" (r0), "1" (r1), "2" (r2), "3" (r3), \
      "r" (r4), "r" (r5), "r" (r6), "r" (r7) \
    : "memory"

#define FIASCO_ARM_ASM_REG(n) asm("r" # n)

INTERFACE [arm && 64bit]:

#define FIASCO_ARM_SMC_CALL_ASM_OPERANDS \
    : "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3), \
      "=r" (r4), "=r" (r5), "=r" (r6), "=r" (r7) \
    : "0" (r0), "1" (r1), "2" (r2), "3" (r3), \
      "4" (r4), "5" (r5), "6" (r6), "7" (r7) \
    : "memory", "x8", "x9", "x10", "x11", "x12", "x13", \
      "x14", "x15", "x16", "x17"

#define FIASCO_ARM_ASM_REG(n) asm("x" # n)

