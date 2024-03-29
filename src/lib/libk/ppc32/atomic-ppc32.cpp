IMPLEMENTATION [ppc32]:

#include <panic.h>

inline
bool
local_cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{
  Mword ret = 0;

  asm volatile ( "1:                            \n"
		 "  lwarx  %%r6, 0, %[ptr]      \n"
		 "  cmpw   %[oldval], %%r6      \n"
		 "  bne-   2f                   \n"
		 "  stwcx. %[newval], 0,%[ptr]  \n"
		 "  bne-   1b                   \n"
		 "2:                            \n"
		 "  mr     %[ret], %%r6         \n"
		 : [ret] "=r"(ret),
		   [ptr] "=r"(ptr),
		   [oldval] "=r"(oldval),
		   [newval] "=r"(newval)
		 : "0" (ret),
		   "1" (ptr),
		   "2" (oldval),
		   "3" (newval)
		 : "memory", "r6"
		);

  return ret == oldval;
}

inline
void
local_atomic_and(Mword *l, Mword mask)
{
  Mword old;
  do { old = *l; }
  while (!local_cas(l, old, old & mask));
}

inline
void
local_atomic_or(Mword *l, Mword bits)
{
  Mword old;
  do { old = *l; }
  while (!local_cas(l, old, old | bits));
}

inline
Mword
atomic_add_fetch(Mword *l, Mword a)
{
  Mword old;
  do { old = *l; }
  while (!cas(l, old, old + a));
  return old + a;
}
