IMPLEMENTATION [sparc]:

#include <panic.h>

inline
bool
local_cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
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
  register Mword _ptr        asm("l0") = reinterpret_cast<Mword>(ptr);
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

template<typename T, typename V> requires(sizeof(T) == 4 || sizeof(T) == 8)
inline
void
atomic_store(T *mem, V value)
{
  __atomic_store(reinterpret_cast<T *>(mem), reinterpret_cast<T *>(&value),
                 __ATOMIC_SEQ_CST);
}

template<typename T> requires(sizeof(T) == 4 || sizeof(T) == 8) inline
T
atomic_load(T const *mem)
{
  T res;
  __atomic_load(reinterpret_cast<T const *>(mem), reinterpret_cast<T *>(&res),
                __ATOMIC_SEQ_CST);
  return res;
}
