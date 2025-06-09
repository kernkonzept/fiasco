INTERFACE:

#include "mem.h"
#include "std_macros.h"
#include "processor.h"

EXTENSION class Mmu
{
public:
  static void btc_flush();
  static void btc_inv();
  static void inv_cache();
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v5]:

IMPLEMENT static inline ALWAYS_INLINE
void Mmu::btc_flush()
{}

IMPLEMENT static inline ALWAYS_INLINE
void Mmu::btc_inv()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus && 32bit]:

IMPLEMENT static inline ALWAYS_INLINE
void Mmu::btc_flush()
{ asm volatile ("mcr p15, 0, %0, c7, c5, 6" : : "r" (0) : "memory"); }

IMPLEMENT static inline ALWAYS_INLINE
void Mmu::btc_inv()
{ asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory"); }

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus && 64bit]:

IMPLEMENT static inline ALWAYS_INLINE
void Mmu::btc_flush()
{}

IMPLEMENT static inline ALWAYS_INLINE
void Mmu::btc_inv()
{}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_mpcore || arm_1136 || arm_1176 || arm_pxa
                        || arm_sa || arm_926 || arm_920t)]:

PUBLIC static inline
constexpr Mword Mmu::dcache_line_size()
{
  return 32;
}

PUBLIC static inline
constexpr Mword Mmu::icache_line_size()
{
  return 32;
}

// ---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_920t]:

IMPLEMENT inline ALWAYS_INLINE
void Mmu::flush_cache(void const *, void const *)
{
  flush_cache();
}

IMPLEMENT
void Mmu::clean_dcache(void const *, void const *)
{
  clean_dcache();
}

IMPLEMENT
void Mmu::clean_dcache(void const *)
{
  clean_dcache();
}

IMPLEMENT
void Mmu::flush_dcache(void const *, void const *)
{
  flush_dcache();
}


IMPLEMENT
void Mmu::inv_dcache(void const *, void const *)
{
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

IMPLEMENT
void Mmu::flush_cache(void const *, void const *)
{
  flush_cache();
}

IMPLEMENT
void Mmu::clean_dcache(void const *start, void const *end)
{
  if (reinterpret_cast<Address>(end) - reinterpret_cast<Address>(start) >= 8192)
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
void Mmu::clean_dcache(void const *va)
{
  __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 1       \n"
                        : : "r"(va) : "memory");
}

IMPLEMENT
void Mmu::flush_dcache(void const *start, void const *end)
{
  if (reinterpret_cast<Address>(end) - reinterpret_cast<Address>(start) >= 8192)
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
void Mmu::inv_dcache(void const *start, void const *end)
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
  FIASCO_NO_UNROLL_LOOPS __attribute__((always_inline))
  set_way_full_loop(T const &f)
  {
    Mem::dmb();
    Mword clidr = get_clidr();
    // Level of Coherency CLIDR[26:24] * 2 to simplify
    //   get_ccsidr((cache level << 1) | 0)
    unsigned lvl = ((clidr >> 24) & 7) << 1;

    for (unsigned cl = 0; cl < lvl; cl += 2, clidr >>= 3)
      {
        // data cache only
        // - 0x2 data cache only
        // - 0x3 seperate instruction/data caches
        // - 0x4 unified cache
        if ((clidr & 6) == 0)
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
void Mmu::flush_dcache()
{
  Mem::dsb();
  set_way_full_loop(dc_cisw);
}

IMPLEMENT inline ALWAYS_INLINE
void Mmu::flush_cache()
{
  Mem::dsb();
  ic_iallu();

  set_way_full_loop(dc_cisw);
}

IMPLEMENT
void Mmu::clean_dcache()
{
  Mem::dsb();
  set_way_full_loop(dc_csw);
}

IMPLEMENT
void Mmu::inv_cache()
{
  // No need for a DSB here. The cache must be disabled when calling the
  // function, otherwise dirty data would be lost. Therefore all memory
  // accesses bypass the cache already and we don't have to wait for them to
  // retire.
  ic_iallu();
  set_way_full_loop(dc_isw);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_sa]:

#include "mem_layout.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["mem_layout.h"]
void Mmu::flush_cache()
{
  Mword dummy;
  asm volatile (
      "     add %0, %1, #8192           \n" // 8k flush area
      " 1:  ldr r0, [%1], %2            \n" // 32 bytes cache line size
      "     teq %1, %0                  \n"
      "     bne 1b                      \n"
      "     mov r0, #0                  \n"
      "     mcr  p15, 0, r0, c7, c7, 0  \n"
      "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
      : "=r" (dummy)
      : "r" (Mem_layout::Cache_flush_area), "i" (dcache_line_size())
      : "r0"
      );
}

IMPLEMENT
void Mmu::clean_dcache()
{
  Mword dummy;
  asm volatile (
      "     add %0, %1, #8192 \n" // 8k flush area
      " 1:  ldr r0, [%1], %2  \n"
      "     teq %1, %0        \n"
      "     bne 1b            \n"
      "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
      : "=r" (dummy)
      : "r" (Mem_layout::Cache_flush_area), "i" (dcache_line_size())
      : "r0"
      );
}

IMPLEMENT
void Mmu::flush_dcache()
{
  Mword dummy;
  asm volatile (
      "     add %0, %1, #8192           \n" // 8k flush area
      " 1:  ldr r0, [%1], %2            \n"
      "     teq %1, %0                  \n"
      "     bne 1b                      \n"
      "     mov  r0, #0                 \n"
      "     mcr  p15, 0, r0, c7, c6, 0  \n" // inv D cache
      "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
      : "=r" (dummy)
      : "r" (Mem_layout::Cache_flush_area), "i" (dcache_line_size())
      : "r0"
      );
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_pxa]:

#include "mem_layout.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["mem_layout.h"]
void Mmu::flush_cache()
{
  Mword dummy1, dummy2;
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
     "0" (Mem_layout::Cache_flush_area),
     "1" (2048)
    );
}

IMPLEMENT
void Mmu::clean_dcache()
{
  Mword dummy1, dummy2;
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
     "0" (Mem_layout::Cache_flush_area),
     "1" (2048)
    );
}

IMPLEMENT
void Mmu::flush_dcache()
{
  Mword dummy1, dummy2;
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
     "0" (Mem_layout::Cache_flush_area),
     "1" (2048)
    );
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_920t]:

IMPLEMENT inline ALWAYS_INLINE
void Mmu::flush_cache()
{
  Mem::dsb();

  // clean and invalidate dcache
  for (unsigned long index = 0; index < (1 << (32 - 26)); ++index)
    for (unsigned long seg = 0; seg < 256; seg += 32)
      asm volatile("mcr p15,0,%0,c7,c14,2" : : "r" ((index << 26) | seg));

  // invalidate icache
  asm volatile("mcr p15,0,%0,c7,c5,0" : : "r" (0) : "memory");
}

IMPLEMENT void Mmu::clean_dcache()
{
  flush_cache();
}

IMPLEMENT void Mmu::flush_dcache()
{
  flush_cache();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_926]:

IMPLEMENT inline ALWAYS_INLINE
void Mmu::flush_cache()
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
void Mmu::clean_dcache()
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
void Mmu::flush_dcache()
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
