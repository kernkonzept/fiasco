IMPLEMENTATION [arm && !arm_v6plus]:

inline
bool
cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{
  Mword ret;
  asm volatile("    mrs    r5, cpsr    \n"
               "    mov    r6,r5       \n"
               "    orr    r6,r6,#0xc0 \n"
               "    msr    cpsr_c, r6  \n"
               ""
               "    ldr    r6, [%1]    \n"
               "    cmp    r6, %2      \n"
               "    streq  %3, [%1]    \n"
               "    moveq  %0, #1      \n"
               "    movne  %0, #0      \n"
               "    msr    cpsr_c, r5  \n"
               : "=r" (ret)
               : "r"  (ptr), "r" (oldval), "r" (newval)
               : "r5", "r6", "memory");
  return ret;
}

inline
void
atomic_and(Mword *l, Mword mask)
{
  Mword old;
  do { old = *l; }
  while (!cas(l, old, old & mask));
}

inline
void
atomic_or(Mword *l, Mword bits)
{
  Mword old;
  do { old = *l; }
  while (!cas(l, old, old | bits));
}
