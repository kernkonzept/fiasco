IMPLEMENTATION [arm && !arm_v6plus]:

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

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

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

inline
bool
cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{ return mp_cas_arch(ptr, oldval, newval); }

inline
void
atomic_or(Mword *l, Mword value)
{ atomic_mp_or(l, value); }

inline
void
atomic_and(Mword *l, Mword mask)
{ atomic_mp_and(l, mask); }
