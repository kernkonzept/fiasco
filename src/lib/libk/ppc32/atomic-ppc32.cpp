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
