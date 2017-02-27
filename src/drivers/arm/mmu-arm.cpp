INTERFACE:

#include "mem.h"
#include "std_macros.h"
#include "processor.h"

EXTENSION class Mmu
{
public:
  static void btc_flush();
  static void btc_inv();
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v5]:

IMPLEMENT static inline
template < unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::btc_flush()
{}

IMPLEMENT static inline
template < unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::btc_inv()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_v6 || arm_v7)]:

IMPLEMENT static inline
template < unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::btc_flush()
{ asm volatile ("mcr p15, 0, %0, c7, c5, 6" : : "r" (0) : "memory"); }

IMPLEMENT static inline
template < unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::btc_inv()
{ asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory"); }

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8]:

IMPLEMENT static inline
template < unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::btc_flush()
{}

IMPLEMENT static inline
template < unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::btc_inv()
{}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_mpcore || arm_1136 || arm_1176 || arm_pxa
                        || arm_sa || arm_926 || arm_920t)]:

PUBLIC static inline
template< unsigned long Flush_area, bool Ram >
Mword Mmu<Flush_area, Ram>::dcache_line_size()
{
  return 32;
}

PUBLIC static inline
template< unsigned long Flush_area, bool Ram >
Mword
Mmu<Flush_area, Ram>::icache_line_size()
{
  return 32;
}

// ---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_920t]:

IMPLEMENT inline
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_cache(void const * /*start*/,
				       void const * /*end*/)
{
  flush_cache();
}

IMPLEMENT
template< unsigned long Flush_area , bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::clean_dcache(void const *start, void const *end)
{
  (void)start; (void)end;
  clean_dcache();
}

IMPLEMENT
template< unsigned long Flush_area , bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache(void const *va)
{
  (void)va;
  clean_dcache();
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_dcache(void const *start, void const *end)
{
  (void)start; (void)end;
  flush_dcache();
}


IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::inv_dcache(void const *start, void const *end)
{
  (void)start; (void)end;
  // clean && invalidate dcache  ||| XXX: all
#if 1
  for (unsigned long index = 0; index < (1 << (32 - 26)); ++index)
    for (unsigned long seg = 0; seg < 256; seg += 32)
      asm volatile("mcr p15,0,%0,c7,c14,2" : : "r" ((index << 26) | seg));
#else
  // invalidate dcache --- all
  asm volatile("mcr p15,0,%0,c7,c6,0" : : "r" (0) : "memory");
#endif
}

// ---------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_pxa || arm_sa || arm_926)]:

IMPLEMENT inline
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_cache(void const * /*start*/,
				       void const * /*end*/)
{
  flush_cache();
}

IMPLEMENT
template< unsigned long Flush_area , bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::clean_dcache(void const *start, void const *end)
{
  if (((Address)end) - ((Address)start) >= 8192)
    clean_dcache();
  else
    {
      asm volatile (
	  "    bic  %0, %0, %2 - 1         \n"
	  "1:  mcr  p15, 0, %0, c7, c10, 1 \n"
	  "    add  %0, %0, %2             \n"
	  "    cmp  %0, %1                 \n"
	  "    blo  1b                     \n"
	  "    mcr  p15, 0, %0, c7, c10, 4 \n" // drain WB
	  : : "r" (start), "r" (end), "i" (dcache_line_size())
	  );
    }
}

IMPLEMENT
template< unsigned long Flush_area , bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache(void const *va)
{
  __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 1       \n"
                        : : "r"(va) : "memory");
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_dcache(void const *start, void const *end)
{
  if (((Address)end) - ((Address)start) >= 8192)
    flush_dcache();
  else
    {
      asm volatile (
	  "    bic  %0, %0, %2 - 1         \n"
	  "1:  mcr  p15, 0, %0, c7, c14, 1 \n"
	  "    add  %0, %0, %2             \n"
	  "    cmp  %0, %1                 \n"
	  "    blo  1b                     \n"
	  "    mcr  p15, 0, %0, c7, c10, 4 \n" // drain WB
	  : : "r" (start), "r" (end), "i" (dcache_line_size())
	  );
    }
}


IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::inv_dcache(void const *start, void const *end)
{
  asm volatile (
	  "    bic  %0, %0, %2 - 1         \n"
	  "1:  mcr  p15, 0, %0, c7, c6, 1  \n"
	  "    add  %0, %0, %2             \n"
	  "    cmp  %0, %1                 \n"
	  "    blo  1b                     \n"
	  : : "r" (start), "r" (end), "i" (dcache_line_size())
	  );
}

//-----------------------------------------------------------------------------
INTERFACE [arm_v7 || arm_v8]:

EXTENSION class Mmu
{
private:
  template< typename T >
  static void
  __attribute__((optimize("no-unroll-loops"), always_inline))
  set_way_full_loop(T const &f)
  {
    Mem::dmb();
    Mword clidr = get_clidr();
    unsigned lvl = (clidr >> 23) & 14;

    for (unsigned cl = 0; cl < lvl; cl += 2)
      {
        // data cache only
        if (((clidr >> (cl + (cl / 2))) & 6) == 0)
          continue;

        Mword ccsidr = get_ccsidr(cl);

        unsigned assoc       = ((ccsidr >> 3) & 0x3ff);
        unsigned w_shift     = __builtin_clz(assoc);
        unsigned set         = ((ccsidr >> 13) & 0x7fff);
        unsigned log2linelen = (ccsidr & 7) + 4;
        do
          {
            unsigned w = assoc;
            do
              f((w << w_shift) | (set << log2linelen) | cl);
            while (w--);
          }
        while (set--);
      }

    btc_inv();
    Mem::dsb();
    Mem::isb();
  }
};

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm_v7 || arm_v8]:

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_dcache()
{
  Mem::dsb();
  set_way_full_loop(dc_cisw);
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_cache()
{
  Mem::dsb();
  ic_iallu();

  set_way_full_loop(dc_cisw);
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache()
{
  Mem::dsb();
  set_way_full_loop(dc_csw);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_sa]:

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_cache()
{
  register Mword dummy;
  asm volatile (
      "     add %0, %1, #8192           \n" // 8k flush area
      " 1:  ldr r0, [%1], %2            \n" // 32 bytes cache line size
      "     teq %1, %0                  \n"
      "     bne 1b                      \n"
      "     mov r0, #0                  \n"
      "     mcr  p15, 0, r0, c7, c7, 0  \n"
      "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
      : "=r" (dummy)
      : "r" (Flush_area), "i" (dcache_line_size())
      : "r0"
      );
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::clean_dcache()
{
  register Mword dummy;
  asm volatile (
      "     add %0, %1, #8192 \n" // 8k flush area
      " 1:  ldr r0, [%1], %2  \n"
      "     teq %1, %0        \n"
      "     bne 1b            \n"
      "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
      : "=r" (dummy)
      : "r" (Flush_area), "i" (dcache_line_size())
      : "r0"
      );
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_dcache()
{
  register Mword dummy;
  asm volatile (
      "     add %0, %1, #8192           \n" // 8k flush area
      " 1:  ldr r0, [%1], %2            \n"
      "     teq %1, %0                  \n"
      "     bne 1b                      \n"
      "     mov  r0, #0                 \n"
      "     mcr  p15, 0, r0, c7, c6, 0  \n" // inv D cache
      "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
      : "=r" (dummy)
      : "r" (Flush_area), "i" (dcache_line_size())
      : "r0"
      );
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_pxa]:

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_cache()
{
  register Mword dummy1, dummy2;
  asm volatile
    (
     // write back data cache
     " 1: mcr p15, 0, %0, c7, c2, 5                      \n\t"
     "    add %0, %0, #32                                \n\t"
     "    subs %1, %1, #1                                \n\t"
     "    bne 1b                                         \n\t"
     // drain write buffer
     "    mcr  p15, 0, %0, c7, c7, 0                     \n\t"
     "    mcr p15, 0, r0, c7, c10, 4                     \n\t"
     :
     "=r" (dummy1),
     "=r" (dummy2)
     :
     "0" (Flush_area),
     "1" (Ram ? 2048 : 1024)
    );
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::clean_dcache()
{
  register Mword dummy1, dummy2;
  asm volatile
    (
     // write back data cache
     " 1: mcr p15, 0, %0, c7, c2, 5                      \n\t"
     "    add %0, %0, #32                                \n\t"
     "    subs %1, %1, #1                                \n\t"
     "    bne 1b                                         \n\t"
     // drain write buffer
     "    mcr p15, 0, r0, c7, c10, 4                     \n\t"
     :
     "=r" (dummy1),
     "=r" (dummy2)
     :
     "0" (Flush_area),
     "1" (Ram ? 2048 : 1024)
    );
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_dcache()
{
  register Mword dummy1, dummy2;
  asm volatile
    (
     // write back data cache
     " 1: mcr p15, 0, %0, c7, c2, 5                      \n\t"
     "    add %0, %0, #32                                \n\t"
     "    subs %1, %1, #1                                \n\t"
     "    bne 1b                                         \n\t"
     "    mcr  p15, 0, %0, c7, c6, 0                     \n\t" // inv D cache
     // drain write buffer
     "    mcr p15, 0, r0, c7, c10, 4                     \n\t"
     :
     "=r" (dummy1),
     "=r" (dummy2)
     :
     "0" (Flush_area),
     "1" (Ram ? 2048 : 1024)
    );
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_920t]:

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_cache()
{
  Mem::dsb();

  // clean and invalidate dcache
  for (unsigned long index = 0; index < (1 << (32 - 26)); ++index)
    for (unsigned long seg = 0; seg < 256; seg += 32)
      asm volatile("mcr p15,0,%0,c7,c14,2" : : "r" ((index << 26) | seg));

  // invalidate icache
  asm volatile("mcr p15,0,%0,c7,c5,0" : : "r" (0) : "memory");
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::clean_dcache()
{
  flush_cache();
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_dcache()
{
  flush_cache();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_926]:

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_cache()
{
  asm volatile
    (
     // write back data cache
     "1:  mrc p15, 0, r15, c7, c14, 3                    \n\t"
     "    bne 1b                                         \n\t"
     // drain write buffer
     "    mcr p15, 0, %0, c7, c7, 0                      \n\t"
     "    mcr p15, 0, %0, c7, c10, 4                     \n\t"
     : :
     "r" (0)
    );
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::clean_dcache()
{
  asm volatile
    (
     // write back data cache
     "1:  mrc p15, 0, r15, c7, c14, 3                    \n\t"
     "    bne 1b                                         \n\t"
     // drain write buffer
     "    mcr p15, 0, %0, c7, c10, 4                     \n\t"
     : :
     "r" (0)
    );
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
FIASCO_NOINLINE void Mmu<Flush_area, Ram>::flush_dcache()
{
  asm volatile
    (
     // write back data cache
     "1:  mrc p15, 0, r15, c7, c14, 3                    \n\t"
     "    bne 1b                                         \n\t"
     "    mcr  p15, 0, %0, c7, c6, 0                     \n\t" // inv D cache
     // drain write buffer
     "    mcr p15, 0, %0, c7, c10, 4                     \n\t"
     : :
     "r" (0)
    );
}
