INTERFACE [mips]:

// empty: should generate a linker error when used
bool cas2_unsafe(Mword *, Mword *, Mword *);

IMPLEMENTATION [mips]:

#include "asm_mips.h"

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

inline NEEDS["asm_mips.h"]
bool
cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{
  Mword ret;

  __asm__ __volatile__(
      "     .set    push                \n"
      "     .set    noat    #CAS        \n"
      "     .set    noreorder           \n"
      "1:   " ASM_LL " %[ret], %[ptr]   \n"
      "     bne     %[ret], %z[old], 2f \n"
      "       move $1, %z[newval]       \n"
      "     " ASM_SC " $1, %[ptr]       \n"
      "     beqz    $1, 1b              \n"
      "       nop                       \n"
      "2:                               \n"
      "     .set    pop                 \n"
      : [ret] "=&r" (ret), [ptr] "+ZC" (*ptr)
      : [old] "Jr" (oldval), [newval] "Jr" (newval));

  // true is ok
  // false is failed
  return ret == oldval;
}

inline NEEDS["asm_mips.h"]
void
atomic_and(Mword *l, Mword mask)
{
  Mword tmp;

  do
    {
      __asm__ __volatile__(
          ASM_LL " %[tmp], %[ptr]  \n"
          "and     %[tmp], %[mask] \n"
          ASM_SC " %[tmp], %[ptr]  \n"
          : [tmp] "=&r" (tmp), [ptr] "+ZC" (*l)
          : [mask] "Ir" (mask));
    }
  while (!tmp);
}

inline NEEDS["asm_mips.h"]
void
atomic_or(Mword *l, Mword bits)
{
  Mword tmp;

  do
    {
      __asm__ __volatile__(
          ASM_LL "  %[tmp], %[ptr]  \n"
          "or       %[tmp], %[bits] \n"
          ASM_SC "  %[tmp], %[ptr]  \n"
          : [tmp] "=&r" (tmp), [ptr] "+ZC" (*l)
          : [bits] "Ir" (bits));
    }
  while (!tmp);
}

inline NEEDS["asm_mips.h"]
void
atomic_add(Mword *l, Mword v)
{
  Mword tmp;

  do
    {
      __asm__ __volatile__(
          ASM_LL   " %[tmp], %[ptr]  \n"
          ASM_ADDU " %[tmp], %[v]    \n"
          ASM_SC   " %[tmp], %[ptr]  \n"
          : [tmp] "=&r" (tmp), [ptr] "+ZC" (*l)
          : [v] "Jg" (v));
    }
  while (!tmp);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [mips && mp]:

#include "mem.h"

// empty: should generate a linker error when used
template<typename W> inline
bool mp_cas2_arch(void *, W, Mword, Mword, Mword)
{
  W::no_cas2 = 10; // trigger a compile error if cas2 is used
  return false;
}

inline NEEDS["mem.h"]
void
atomic_mp_and(Mword *l, Mword value)
{
  Mem::mp_mb();
  atomic_and(l, value);
  Mem::mp_mb();
}

inline NEEDS["mem.h"]
void
atomic_mp_or(Mword *l, Mword value)
{
  Mem::mp_mb();
  atomic_or(l, value);
  Mem::mp_mb();
}

inline NEEDS["mem.h"]
void
atomic_mp_add(Mword *l, Mword value)
{
  Mem::mp_mb();
  atomic_add(l, value);
  Mem::mp_mb();
}

inline NEEDS["mem.h"]
bool
mp_cas_arch(Mword *m, Mword o, Mword n)
{
  Mem::mp_mb();
  Mword ret = cas_unsafe(m, o, n);
  Mem::mp_mb();
  return ret;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [mips && !mp]:

inline
void
atomic_mp_and(Mword *l, Mword value)
{ atomic_and(l, value); }

inline
void
atomic_mp_or(Mword *l, Mword value)
{ atomic_or(l, value); }

inline
bool
mp_cas_arch(Mword *m, Mword o, Mword n)
{ return cas_unsafe(m, o, n); }

