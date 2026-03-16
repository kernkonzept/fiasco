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

template<typename T, typename V> ALWAYS_INLINE inline
void
atomic_store(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  switch (sizeof(T))
    {
    case 4:
      __atomic_store(reinterpret_cast<Unsigned32 *>(mem),
	             reinterpret_cast<Unsigned32 *>(&value),
		     __ATOMIC_SEQ_CST);
      break;

    case 8:
      __atomic_store(reinterpret_cast<Unsigned64 *>(mem),
	             reinterpret_cast<Unsigned64 *>(&value),
		     __ATOMIC_SEQ_CST);
      break;
    }
}

template<typename T> ALWAYS_INLINE inline
T
atomic_load(T const *mem)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T res;
  switch (sizeof(T))
    {
    case 4:
      __atomic_load(reinterpret_cast<Unsigned32 const *>(mem),
	            reinterpret_cast<Unsigned32 *>(&res),
		    __ATOMIC_SEQ_CST);
      return res;

    case 8:
      __atomic_load(reinterpret_cast<Unsigned64 const *>(mem),
	            reinterpret_cast<Unsigned64 *>(&res),
		    __ATOMIC_SEQ_CST);
      return res;
    }
}
