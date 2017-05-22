INTERFACE [arm]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Phys_layout : Address {
    Sdram_phys_base      = RAM_PHYS_BASE
  };

  static inline unsigned long round_superpage(unsigned long addr)
  { return (addr + Config::SUPERPAGE_SIZE - 1) & ~(Config::SUPERPAGE_SIZE-1); }
  static inline unsigned long trunc_superpage(unsigned long addr)
  { return addr & ~(Config::SUPERPAGE_SIZE-1); }
};

// -------------------------------------------------------------------------
IMPLEMENTATION [arm]:

//---------------------------------
// Workaround GCC BUG 33661
// Do not use register asm ("r") in a template function, it will be ignored
//---------------------------------
PUBLIC static inline
bool
Mem_layout::_read_special_safe(Mword const *address, Mword &v)
{
  register Mword a asm("r14") = (Mword)address;
  Mword ret;
  asm volatile ("msr cpsr_f, #0    \n" // clear flags
                "ldr %[a], [%[a]]  \n"
		"movne %[ret], #1      \n"
		"moveq %[ret], #0      \n"

                : [a] "=r" (a), [ret] "=r" (ret)
                : "0" (a)
                : "cc");
  v = a;
  return ret;
}

PUBLIC static inline
template< typename V >
bool
Mem_layout::read_special_safe(V const *address, V &v)
{
  Mword _v;
  bool ret = _read_special_safe(reinterpret_cast<Mword const*>(address), _v);
  v = V(_v);
  return ret;
}

PUBLIC static inline
template< typename T >
T
Mem_layout::read_special_safe(T const *a)
{
  return T(_read_special_safe((Mword const *)a));
#if 0
  Mword res;
  asm volatile ("msr cpsr_f, #0; ldr %0, [%1]; moveq %0, #0\n"
		: "=r" (res) : "r" (a) : "cc");
  return T(res);
#endif
}
