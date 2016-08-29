INTERFACE [arm]:

#include "mem_layout.h"

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
IMPLEMENTATION [arm && armv5]:

static inline void Bootstrap::set_asid() {}
static inline void Bootstrap::set_ttbcr() {}

//---------------------------------------------------------------------------
INTERFACE [arm && (armv6 || armv7)]:

#include "kmem_space.h"

static inline void
Bootstrap::set_asid()
{
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r" (0)); // ASID 0
}

static inline NEEDS["kmem_space.h"]
void
Bootstrap::set_ttbcr()
{
  asm volatile("mcr p15, 0, %[ttbcr], c2, c0, 2" // TTBCR
               : : [ttbcr] "r" (Page::Ttbcr_bits));
}


//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm1176_cache_alias_fix]:

PUBLIC static inline
void
Bootstrap::do_arm_1176_cache_alias_workaround() {}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae && !(armv7 || mpcore)]:

PUBLIC static inline
Bootstrap::Phys_addr
Bootstrap::init_paging(void *const page_dir)
{
  return Phys_addr((Mword)page_dir);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "kmem_space.h"
#include "mmu.h"
#include "globalconfig.h"

extern char bootstrap_bss_start[];
extern char bootstrap_bss_end[];
extern char __bss_start[];
extern char __bss_end[];
extern char kernel_page_directory[] __attribute__((weak));

extern "C" void _start_kernel(void);
extern "C" void bootstrap_main()
{
  void *const page_dir = kernel_page_directory + Bootstrap::Virt_ofs;

  Unsigned32 tbbr = cxx::int_value<Bootstrap::Phys_addr>(Bootstrap::init_paging(page_dir))
                    | Page::Ttbr_bits;

  Bootstrap::create_initial_mappings(page_dir);

  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  Bootstrap::do_arm_1176_cache_alias_workaround();
  Bootstrap::enable_paging(tbbr);

  Bootstrap::add_initial_pmem();

  _start_kernel();

  while(1)
    ;
}

