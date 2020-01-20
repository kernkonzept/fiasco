IMPLEMENTATION [arm && debug]:

#include "types.h"

inline NEEDS["types.h"]  void kdb_ke(char const *msg)
{
  register Mword x0 asm("x0") = (Mword)msg;
  asm volatile ("brk #0" : : "r"(x0));
}

inline NEEDS["types.h"] void kdb_ke_nstr(char const *msg, unsigned len)
{
  register Mword x0 asm("x0") = (Mword)msg;
  register Mword x1 asm("x1") = len;
  asm volatile ("brk #1" : : "r"(x0), "r"(x1));
}

inline NEEDS["types.h"] void kdb_ke_sequence(char const *msg, unsigned len)
{
  register Mword x0 asm("x0") = (Mword)msg;
  register Mword x1 asm("x1") = len;
  asm volatile ("brk #2" : : "r"(x0), "r"(x1));
}
