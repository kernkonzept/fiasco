IMPLEMENTATION [sparc]:

#include <panic.h>

inline
bool
cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{
  Mword ret;
  // -mcpu=leon3 does not work for me although listed in docs :(
#if 0
  asm volatile("casa [%[ptr]]0xb, %[oldval], %[newval]"
	       : [ptr] "=r" (ptr),
	         [oldval] "=r" (oldval),
	         [newval] "=r" (ret)
               : "0" (ptr),
	         "1" (oldval),
		 "2" (newval));
#else
  register Mword _ptr        asm("l0") = (Mword)ptr;
  register Mword _oldval     asm("l1") = oldval;
  register Mword _newval_ret asm("l2") = newval;
  asm volatile(".word 0xe5e40171\n" // "casa [%%l0]0xb, %%l1, %%l2\n"
               : [ptr] "=r" (_ptr),
                 [oldval] "=r" (_oldval),
                 [newval_ret] "=r" (_newval_ret),
	         [mem] "=m" (*ptr)
               : "0" (_ptr),
                 "1" (_oldval),
                 "2" (_newval_ret));
#endif

  return oldval == _newval_ret;
  return oldval == ret;

  //return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

inline
void
atomic_and(Mword *l, Mword mask)
{
  Mword old;
  do
    old = *l;
  while (!cas(l, old, old & mask));
}

inline
void
atomic_or(Mword *l, Mword bits)
{
  Mword old;
  do
    old = *l;
  while (!cas(l, old, old | bits));
}
