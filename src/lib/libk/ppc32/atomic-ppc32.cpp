IMPLEMENTATION [ppc32]:

#include <panic.h>

inline
bool
cas_unsafe( Mword *ptr, Mword oldval, Mword newval )
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

/* dummy implement */
bool
cas2_unsafe( Mword *, Mword *, Mword *)
{
  panic("%s not implemented", __func__);
  return false;
}

inline
void
atomic_and (Mword *l, Mword mask)
{
  Mword old;
  do { old = *l; }
  while ( !cas (l, old, old & mask));
}

inline
void
atomic_or (Mword *l, Mword bits)
{
  Mword old;
  do { old = *l; }
  while ( !cas (l, old, old | bits));
}
