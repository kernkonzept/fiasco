INTERFACE [arm]:

EXTENSION class Mem
{
public:
  static void prefetch_w(void *addr);
};

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT_DEFAULT inline void Mem::prefetch_w(void *) {}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit]:

IMPLEMENT static inline
void
Mem::memset_mwords (void *dst, const unsigned long value, unsigned long nr_of_mwords)
{
  if (!nr_of_mwords)
    return;

  typedef unsigned long __attribute__((may_alias)) U32;
  typedef unsigned long long __attribute__((may_alias)) U64;

  U32 *d32 = (U32 *)dst;
  if ((unsigned long)d32 & 0x4U)
    {
      *d32++ = value;
      nr_of_mwords--;
    }

  U64 v64 = ((U64)value << 32) | value;
  for (; nr_of_mwords >= 4; d32 += 4, nr_of_mwords -= 4)
    {
      ((U64 *)d32)[0] = v64;
      ((U64 *)d32)[1] = v64;
    }

  if (nr_of_mwords & 0x2U)
    {
      *((U64 *)d32) = v64;
      d32 += 2;
    }

  if (nr_of_mwords & 0x1U)
    *d32 = value;
}

IMPLEMENT static inline
void
Mem::memcpy_mwords (void *dst, void const *src, unsigned long nr_of_mwords)
{
  unsigned long __attribute__((may_alias)) *s = (unsigned long *)src;
  unsigned long __attribute__((may_alias)) *d = (unsigned long *)dst;
  unsigned long tmp;

  if (__builtin_constant_p(nr_of_mwords))
    {
      // Exploit the fact that the length is a compile time constant to pick
      // the best inline assembly.
      switch (nr_of_mwords & 0x03U)
        {
        case 0:
          break;
        case 1:
          asm volatile("ldr %0, [%1], #4\n"
                       "str %0, [%2], #4\n"
                       : "=&r"(tmp), "+r"(s), "+r"(d)
                       : : "memory");
          break;
        case 2:
          asm volatile("ldm %0!, {r0, r1}\n"
                       "stm %1!, {r0, r1}\n"
                       : "+&r"(s), "+&r"(d)
                       : : "r0", "r1", "memory");
          break;
        case 3:
          asm volatile("ldm %0!, {r0, r1, r2}\n"
                       "stm %1!, {r0, r1, r2}\n"
                       : "+&r"(s), "+&r"(d)
                       : : "r0", "r1", "r2", "memory");
          break;
        }
      // nr_of_mwords &= ~0x03UL; not necessary because code below shifts out
      // the two LSBs anyway
    }
  else
    while (nr_of_mwords & 0x03U)
      {
        *d++ = *s++;
        nr_of_mwords--;
      }

  // Need to use inline assembly here because the compiler is not smart enough
  // to use ldm/stm. We could let the compiler choose the scratch registers but
  // it results in assembler warnings because the order of the registers is
  // sometimes not ascending. Using the caller-saved registers emitted the best
  // code overall.
  nr_of_mwords >>= 2;
  while (nr_of_mwords--)
    {
      asm volatile("ldm %0!, {r0, r1, r2, r3}\n"
                   "stm %1!, {r0, r1, r2, r3}\n"
                   : "+&r"(s), "+&r"(d)
                   : : "r0", "r1", "r2", "r3", "memory");
    }
}

IMPLEMENT static inline
void
Mem::memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
{
  __builtin_memcpy(dst, src, nr_of_bytes);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

IMPLEMENT static inline
void
Mem::memset_mwords (void *dst, const unsigned long value, unsigned long nr_of_mwords)
{
  if (!nr_of_mwords)
    return;

  unsigned long __attribute__((may_alias)) *d = (unsigned long *)dst;
  for (; nr_of_mwords >= 4; d += 4, nr_of_mwords -= 4U)
    {
      d[0] = value;
      d[1] = value;
      d[2] = value;
      d[3] = value;
    }

  if (nr_of_mwords & 0x2U)
    {
      d[0] = value;
      d[1] = value;
      d += 2;
    }

  if (nr_of_mwords & 0x1U)
    *d = value;
}

IMPLEMENT static inline
void
Mem::memcpy_mwords (void *dst, void const *src, unsigned long nr_of_mwords)
{
  // The C-version is good enough.
  __builtin_memcpy(dst, src, nr_of_mwords * sizeof(unsigned long));
}

IMPLEMENT static inline
void
Mem::memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
{
  __builtin_memcpy(dst, src, nr_of_bytes);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm_v5]:

PUBLIC static inline void Mem::dmbst() { barrier(); }

PUBLIC static inline void Mem::dmb() { barrier(); }

PUBLIC static inline void Mem::isb() { barrier(); }

PUBLIC static inline void Mem::dsb()
{ __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm_v6]:

PUBLIC static inline void Mem::dmbst()
{ __asm__ __volatile__ ("mcr p15, 0, r0, c7, c10, 5" : : : "memory"); }

PUBLIC static inline void Mem::dmb()
{ __asm__ __volatile__ ("mcr p15, 0, r0, c7, c10, 5" : : : "memory"); }

PUBLIC static inline void Mem::isb()
{ __asm__ __volatile__ ("mcr p15, 0, r0, c7, c5, 4" : : : "memory"); }

PUBLIC static inline void Mem::dsb()
{ __asm__ __volatile__ ("mcr p15, 0, r0, c7, c10, 4" : : : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [32bit && ((arm_v7 && mp) || arm_v8)]:

IMPLEMENT_OVERRIDE inline void
Mem::prefetch_w(void *addr)
{
  asm (".arch_extension mp\n"
       "pldw %0" : : "m"(*reinterpret_cast<char *>(addr)));
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm_v7 || arm_v8]:

PUBLIC static inline void Mem::dmbst()
{ __asm__ __volatile__ ("dmb ishst" : : : "memory"); }

PUBLIC static inline void Mem::dmb()
{ __asm__ __volatile__ ("dmb ish" : : : "memory"); }

PUBLIC static inline void Mem::isb()
{ __asm__ __volatile__ ("isb sy" : : : "memory"); }

PUBLIC static inline void Mem::dsb()
{ __asm__ __volatile__ ("dsb ish" : : : "memory"); }

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && mp]:

IMPLEMENT inline static void Mem::mb() { dmb(); }
IMPLEMENT inline static void Mem::rmb() { dmb(); }
IMPLEMENT inline static void Mem::wmb() { dmbst(); }

IMPLEMENT inline static void Mem::mp_mb() { dmb(); }
IMPLEMENT inline static void Mem::mp_acquire() { dmb(); }
IMPLEMENT inline static void Mem::mp_release() { dmb(); }
IMPLEMENT inline static void Mem::mp_rmb() { dmb(); }
IMPLEMENT inline static void Mem::mp_wmb() { dmbst(); }

