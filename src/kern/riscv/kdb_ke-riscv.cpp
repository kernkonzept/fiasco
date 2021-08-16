IMPLEMENTATION [riscv && debug]:

#include "types.h"

inline NEEDS["types.h"]
void
kdb_ke(char const *msg)
{
  register Mword a0 asm("a0") = 0;
  register char const *a1 asm("a1") = msg;
  __asm__ __volatile__ ("ebreak" : : "r"(a0), "r"(a1));
}

inline NEEDS["types.h"]
void
kdb_ke_nstr(char const *msg, unsigned len)
{
  register Mword a0 asm("a0") = 1;
  register char const *a1 asm("a1") = msg;
  register unsigned a2 asm("a2") = len;
  __asm__ __volatile__ ("ebreak" : : "r"(a0), "r"(a1), "r"(a2));
}


inline NEEDS["types.h"]
void
kdb_ke_sequence(char const *msg, unsigned len)
{
  register Mword a0 asm("a0") = 2;
  register char const *a1 asm("a1") = msg;
  register unsigned a2 asm("a2") = len;
  __asm__ __volatile__ ("ebreak" : : "r"(a0), "r"(a1), "r"(a2));
}
