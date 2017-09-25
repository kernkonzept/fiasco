//-----------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT static inline
void
Mem::memset_mwords (void *dst, const unsigned long value, unsigned long nr_of_mwords)
{
  unsigned long *d = (unsigned long *)dst;
  for (; nr_of_mwords--; d++)
    *d = value;
}

IMPLEMENT static inline
void
Mem::memcpy_mwords (void *dst, void const *src, unsigned long nr_of_mwords)
{
  __builtin_memcpy(dst, src, nr_of_mwords * sizeof(unsigned long));
}

IMPLEMENT static inline
void
Mem::memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
{
  __builtin_memcpy(dst, src, nr_of_bytes);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [armv5]:

PUBLIC static inline void Mem::dmb() { barrier(); }

PUBLIC static inline void Mem::isb() { barrier(); }

PUBLIC static inline void Mem::dsb()
{ __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [armv6]:

PUBLIC static inline void Mem::dmb()
{ __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory"); }

PUBLIC static inline void Mem::isb()
{ __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" : : "r" (0) : "memory"); }

PUBLIC static inline void Mem::dsb()
{ __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [armv7]:

PUBLIC static inline void Mem::dmb()
{ __asm__ __volatile__ ("dmb" : : : "memory"); }

PUBLIC static inline void Mem::isb()
{ __asm__ __volatile__ ("isb" : : : "memory"); }

PUBLIC static inline void Mem::dsb()
{ __asm__ __volatile__ ("dsb" : : : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && mp]:

IMPLEMENT inline static void Mem::mb() { dmb(); }
IMPLEMENT inline static void Mem::rmb() { dmb(); }
IMPLEMENT inline static void Mem::wmb() { dmb(); }

IMPLEMENT inline static void Mem::mp_mb() { dmb(); }
IMPLEMENT inline static void Mem::mp_acquire() { dmb(); }
IMPLEMENT inline static void Mem::mp_release() { dmb(); }
IMPLEMENT inline static void Mem::mp_rmb() { dmb(); }
IMPLEMENT inline static void Mem::mp_wmb() { dmb(); }

