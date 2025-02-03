//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v7plus]:

PUBLIC static inline
bool
Page::is_attribs_upgrade_safe(Page::Attr old_attr, Page::Attr new_attr)
{
  // Break-before-make sequence is necessary if memory type or Global flag
  // are changed.
  return old_attr.type == new_attr.type && old_attr.kern == new_attr.kern;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_v5 || arm_v6)]:

PUBLIC static inline
bool
Page::is_attribs_upgrade_safe(Page::Attr, Page::Attr)
{
  // No break-before-make required on ARMv5 and ARMv6, thus all attribute
  // upgrades are considered safe.
  return true;
}

//-------------------------------------------------------------------------------------
INTERFACE [arm && mmu && !arm_lpae]:

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits      = 0,
    /**
     * Primary Region Remap (PRRR)
     * TR0, NOS0: Device-nGnRnE memory, Inner Shareable
     * TR1, NOS1: Normal memory, Inner Shareable
     * TR2, NOS2: Normal memory, Inner Shareable
     */
    Mair0_prrr_bits = 0xff0a0028,
    /**
     * Normal Memory Remap (NMRR)
     * IR1/OR1: Inner/Outer Non-cacheable
     * IR2/OR2: Inner/Outer Write-Back Write-Allocate Cacheable
     */
    Mair1_nmrr_bits = 0x00100010,
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae]:

EXTENSION class Page
{
public:
  enum
  {
    /// Attributes for page-table walks
    Tcr_attribs =  (3UL << 4)  // SH0
                 | (1UL << 2)  // ORGN0
                 | (1UL << 0), // IRGN0

    /**
     * Memory Attribute Indirection (MAIR0)
     * Attr0: Device-nGnRnE memory
     * Attr1: Normal memory, Inner/Outer Non-cacheable
     * Attr2: Normal memory, RW, Inner/Outer Write-Back Cacheable (Non-transient)
     * Attr3: Device-nGnRnE memory (unused)
     */
    Mair0_prrr_bits = 0x00ff4400,
    /**
     * Memory Attribute Indirection (MAIR1)
     * Attr4..Attr7: Device-nGnRnE memory (unused)
     */
    Mair1_nmrr_bits = 0,
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_v5]:

#include "types.h"

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    Cache_mask    = 0x0c,
    NONCACHEABLE  = 0x00, ///< Caching is off
    CACHEABLE     = 0x0c, ///< Cache is enabled

    // The next are ARM specific
    WRITETHROUGH = 0x08, ///< Write through cached
    BUFFERED     = 0x04, ///< Write buffer enabled
  };

  enum Default_entries : Mword
  {
    Section_cachable = 0x40e,
    Section_no_cache = 0x402,
    Section_local    = 0,
    Section_global   = 0,
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && ((arm_v6plus && mp) || arm_v8)]:

EXTENSION class Page
{
public:
  enum
  {
    Section_shared = 1UL << 16,
    Mp_set_shared = 0x400,
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && (arm_v6 || arm_v7) && !mp]:

EXTENSION class Page
{
public:
  enum
  {
    Section_shared = 0,
    Mp_set_shared = 0,
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && (arm_v5 || (arm_v6 && !arm_mpcore))]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0 }; };

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_mpcore]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0xa }; };

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && ((mp && arm_v7) || arm_v8) && !arm_lpae]:

// S Sharable | RGN = Outer WB-WA | IRGN = Inner WB-WA | NOS
EXTENSION class Page
{ public: enum { Ttbr_bits = 0x6a }; };

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0 }; };

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !mp && arm_v7 && !arm_lpae]:
// armv7 w/o multiprocessing ext.

// RGN = Outer WB-WA | IRGN = Inner WB-WA
EXTENSION class Page
{ public: enum { Ttbr_bits = 0x09 }; };

//----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_v6 && !arm_mpcore]:

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x144, ///< Cache is enabled
    BUFFERED      = 0x040, ///< Write buffer enabled -- Normal, non-cached
  };

  enum Default_entries : Mword
  {
    Section_cachable_bits = 0x5004,
  };
};

//----------------------------------------------------------------------------
INTERFACE [arm && mmu && ((arm_v6 && arm_mpcore) || ((arm_v7 || arm_v8) && !arm_lpae))]:

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    NONCACHEABLE  = 0x000, ///< PRRR TR0: Caching is off
    CACHEABLE     = 0x008, ///< PRRR TR2: Cache is enabled
    BUFFERED      = 0x004, ///< PRRR TR1: Write buffer enabled -- Normal, non-cached
  };

  enum Default_entries : Mword
  {
    Section_cachable_bits = 8,
  };
};

//----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_v6plus && !arm_lpae]:

#include "types.h"

EXTENSION class Page
{
public:
  enum
  {
    Cache_mask    = 0x1cc,
  };

  enum : Mword
  {
    Section_cache_mask = 0x700c,
    Section_local      = (1 << 17),
    Section_global     = 0,
    Section_cachable   = 0x0402 | Section_shared | Section_cachable_bits,
    Section_no_cache   = 0x0402 | Section_shared | 0x10 /*XN*/,
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae && !cpu_virt]:

#include "types.h"

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    Cache_mask    = 0x01c, ///< MAIR index 0..7
    NONCACHEABLE  = 0x000, ///< MAIR Attr0: Caching is off
    CACHEABLE     = 0x008, ///< MAIR Attr2: Cache is enabled
    BUFFERED      = 0x004, ///< MAIR Attr1: Write buffer enabled -- Normal, non-cached
  };

  /// The EL1&0 translation regime supports two privilege levels.
  enum { Priv_levels = 2 };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae && !cpu_virt && 32bit]:

EXTENSION class Page
{
public:
  enum
  {
    PXN = 1ULL << 53, ///< Privileged Execute Never
    UXN = 0,          ///< Unprivileged Execute Never feature not available
    XN  = 1ULL << 54, ///< Execute Never (all exception levels)
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae && !cpu_virt && 64bit]:

EXTENSION class Page
{
public:
  enum
  {
    PXN = 1ULL << 53, ///< Privileged Execute Never
    UXN = 1ULL << 54, ///< Unprivileged Execute Never
    XN  = 0,          ///< Execute Never feature not available
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae && cpu_virt]:

#include "types.h"

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    Cache_mask    = 0x03c,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x03c, ///< Cache is enabled
    BUFFERED      = 0x014, ///< Write buffer enabled -- Normal, non-cached
  };
};
