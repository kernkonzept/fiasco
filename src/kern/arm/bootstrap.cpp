INTERFACE [arm]:

#include "mem_layout.h"
#include "paging.h"
#include "boot_infos.h"

struct Bootstrap_info
{
  void (*entry)();
  void *kip;
  Boot_paging_info pi;
};

extern Bootstrap_info bs_info;

class Bootstrap
{
public:
  struct Order_t;
  struct Virt_addr_t;

  typedef cxx::int_type<unsigned, Order_t> Order;
  typedef cxx::int_type_order<Mword, Virt_addr_t, Order> Virt_addr;

  enum
  {
    Virt_ofs = Mem_layout::Sdram_phys_base - Mem_layout::Map_base,
  };

};

//---------------------------------------------------------------------------
INTERFACE [arm && !(arm_sa || arm_pxa)]:

EXTENSION class Bootstrap
{
public:
  enum { Cache_flush_area = 0 };
};

//---------------------------------------------------------------------------
INTERFACE [arm && arm_lpae]:

#include <cxx/cxx_int>

EXTENSION class Bootstrap
{
public:
  struct Phys_addr_t;
  typedef cxx::int_type_order<Unsigned64, Phys_addr_t, Order> Phys_addr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Phys_addr
Bootstrap::pt_entry(Phys_addr pa, bool cache, bool local)
{
  Phys_addr res = cxx::mask_lsb(pa, map_page_order()) | Phys_addr(1); // this is a block

  if (local)
    res |= Phys_addr(1 << 11); // nG flag

  if (cache)
    res |= Phys_addr(8);
  else
    res |= Phys_addr(1ULL << 54); // assume XN for non-cachable memory

  res |= Phys_addr(1 << 10); // AF
  res |= Phys_addr(3 << 8);  // Inner sharable
  return res;
}

//---------------------------------------------------------------------------
INTERFACE [arm && !arm_lpae]:

#include <cxx/cxx_int>

EXTENSION class Bootstrap
{
public:
  struct Phys_addr_t;
  typedef cxx::int_type_order<Unsigned32, Phys_addr_t, Order> Phys_addr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

#include "paging.h"

static inline
Bootstrap::Order
Bootstrap::map_page_order() { return Order(20); }

PUBLIC static inline NEEDS["paging.h"]
Bootstrap::Phys_addr
Bootstrap::pt_entry(Phys_addr pa, bool cache, bool local)
{
  return cxx::mask_lsb(pa, map_page_order())
                | Phys_addr(cache ? Page::Section_cachable : Page::Section_no_cache)
                | Phys_addr(local ? Page::Section_local : Page::Section_global);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstddef>
#include "types.h"
#include "cpu.h"
#include "paging.h"

Bootstrap_info FIASCO_BOOT_PAGING_INFO bs_info;

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Phys_addr
Bootstrap::map_page_size_phys()
{ return Phys_addr(1) << map_page_order(); }

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Virt_addr
Bootstrap::map_page_size()
{ return Virt_addr(1) << map_page_order(); }

static inline void
Bootstrap::map_memory(void volatile *pd, Virt_addr va, Phys_addr pa,
                      bool cache, bool local)
{
  Phys_addr *const p = (Phys_addr*)pd;
  p[cxx::int_value<Virt_addr>(va >> map_page_order())]
    = pt_entry(pa, cache, local);
}


//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v5]:

static inline void Bootstrap::set_asid() {}
static inline void Bootstrap::set_ttbcr() {}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_1176_cache_alias_fix]:

PUBLIC static inline
void
Bootstrap::do_arm_1176_cache_alias_workaround() {}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "kmem_space.h"
#include "mmu.h"
#include "globalconfig.h"

PUBLIC static inline ALWAYS_INLINE
void *
Bootstrap::kern_to_boot(void *a)
{
  return (void *)((Mword)a + Bootstrap::Virt_ofs);
}

extern "C" void bootstrap_main()
{
  Unsigned32 tbbr = cxx::int_value<Bootstrap::Phys_addr>(Bootstrap::init_paging())
                    | Page::Ttbr_bits;

  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  Bootstrap::do_arm_1176_cache_alias_workaround();
  Bootstrap::enable_paging(tbbr);

  // force to construct an absolute relocation because GCC may not do it.
  bs_info.entry();

  while(1)
    ;
}

