/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */

IMPLEMENTATION [mips]:

PUBLIC static inline
void
Mem::sync()
{ asm volatile ("sync" : : : "memory"); }

IMPLEMENT static inline
void
Mem::memset_mwords(void *dst, const unsigned long value, unsigned long nr_of_mwords)
{
  unsigned long *d = (unsigned long *)dst;
  for (; nr_of_mwords--; d++)
    *d = value;
}

IMPLEMENT static inline
void
Mem::memcpy_mwords(void *dst, void const *src, unsigned long nr_of_mwords)
{
  // FIXME: check if the compiler generates code that assumes Mword alignment
  __builtin_memcpy(dst, src, nr_of_mwords * sizeof(unsigned long));
}

IMPLEMENT static inline
void
Mem::memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
{
  __builtin_memcpy(dst, src, nr_of_bytes);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [mips && mp && !weak_ordering]:

IMPLEMENT inline static void Mem::mp_mb() { barrier(); }
IMPLEMENT inline static void Mem::mp_rmb() { barrier(); }
IMPLEMENT inline static void Mem::mp_wmb() { barrier(); }
IMPLEMENT inline static void Mem::mp_acquire() { barrier(); }
IMPLEMENT inline static void Mem::mp_release() { barrier(); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [mips && mp && weak_ordering && mips_lw_barriers && !cavium_octeon]:

IMPLEMENT inline static void Mem::mp_mb() { __asm__ __volatile__("sync 0x10" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_rmb() { __asm__ __volatile__("sync 0x13" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_wmb() { __asm__ __volatile__("sync 0x4" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_acquire() { __asm__ __volatile__("sync 0x11" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_release() { __asm__ __volatile__("sync 0x12" : : : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [mips && mp && weak_ordering && !mips_lw_barriers && !cavium_octeon]:

#if 0
// Alex: I disable these implementations as the semantics are questionable and they are
// not used in any MIPS code
IMPLEMENT inline static void Mem::mb() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::rmb() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::wmb() { __asm__ __volatile__("sync" : : : "memory"); }
#endif

IMPLEMENT inline static void Mem::mp_mb() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_rmb() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_wmb() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_acquire() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_release() { __asm__ __volatile__("sync" : : : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [mips && mp && weak_ordering && cavium_octeon]:

IMPLEMENT inline static void Mem::mp_mb() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_rmb() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_wmb() { __asm__ __volatile__("syncw; syncw" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_acquire() { __asm__ __volatile__("sync" : : : "memory"); }
IMPLEMENT inline static void Mem::mp_release() { __asm__ __volatile__("syncw; syncw" : : : "memory"); }

