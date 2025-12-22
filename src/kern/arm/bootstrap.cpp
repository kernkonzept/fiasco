INTERFACE [arm]:

#include "mem_layout.h"
#include "paging.h"
#include "boot_infos.h"

// Written by the linker. See the :bstrap section in kernel.arm.ld.
struct Bootstrap_info
{
  void (*entry)();
  void *kip;
  Address kernel_start_phys;
  Address kernel_end_phys;
  Address kernel_load_addr;
  Boot_paging_info pi;
};

extern Bootstrap_info bs_info;

//---------------------------------------------------------------------------
INTERFACE [arm && mmu]:

class Bootstrap
{
public:
  struct Order_t;
  struct Virt_addr_t;

  typedef cxx::int_type<unsigned, Order_t> Order;
  typedef cxx::int_type_order<Mword, Virt_addr_t, Order> Virt_addr;

  /**
   * Relative load address.
   *
   * Represents the load base address, that is the offset, that was added to
   * the ELF PHDRs when the image was loaded. If Fiasco was loaded at its link
   * address, it will be zero.
   */
  static unsigned long load_addr;
};

/**
 * Helper to apply REL(A) relocations at startup.
 *
 * It requires that relocations are combined in a single section and that only
 * relative relocations exist.
 *
 * \param DYN   The dynamic section entry type
 * \param RELOC The sole relocation entry type (REL or RELA)
 */
template<typename DYN, typename RELOC>
struct Elf
{
  static inline unsigned long
  elf_dynamic_section()
  {
    extern char _DYNAMIC[] __attribute__ ((visibility ("hidden")));
    return reinterpret_cast<unsigned long>(&_DYNAMIC[0]);
  }

  static inline void
  relocate(unsigned long load_addr)
  {
    DYN *dyn = reinterpret_cast<DYN *>(elf_dynamic_section());
    unsigned long relcnt = 0;
    RELOC *rel = nullptr;

    for (int i = 0; dyn[i].tag != 0; i++)
      switch (dyn[i].tag)
        {
        case DYN::Reloc:
          rel = reinterpret_cast<RELOC*>(dyn[i].ptr + load_addr);
          break;
        case DYN::Reloc_count:
          relcnt = dyn[i].val;
          break;
        }

    if (rel && relcnt)
      {
        for (; relcnt; relcnt--, rel++)
          rel->apply(load_addr);
      }
  }
};

//---------------------------------------------------------------------------
INTERFACE [arm && !mmu]:

class Bootstrap
{
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae]:

#include <cxx/cxx_int>

EXTENSION class Bootstrap
{
public:
  struct Phys_addr_t;
  typedef cxx::int_type_order<Unsigned64, Phys_addr_t, Order> Phys_addr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

Bootstrap_info FIASCO_BOOT_PAGING_INFO bs_info;

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && arm_lpae]:

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Phys_addr
Bootstrap::pt_entry(Phys_addr pa, bool local)
{
  Phys_addr res = cxx::mask_lsb(pa, map_page_order()) | Phys_addr(1); // this is a block

  if (local)
    res |= Phys_addr(1 << 11); // nG flag

  res |= Phys_addr(8); // Cached
  res |= Phys_addr(1 << 10); // AF
  res |= Phys_addr(3 << 8);  // Inner sharable
  return res;
}

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !arm_lpae]:

#include <cxx/cxx_int>

EXTENSION class Bootstrap
{
public:
  struct Phys_addr_t;
  typedef cxx::int_type_order<Unsigned32, Phys_addr_t, Order> Phys_addr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && !arm_lpae]:

#include "paging.h"

static inline
Bootstrap::Order
Bootstrap::map_page_order() { return Order(20); }

PUBLIC static inline NEEDS["paging.h"]
Bootstrap::Phys_addr
Bootstrap::pt_entry(Phys_addr pa, bool local)
{
  return cxx::mask_lsb(pa, map_page_order())
                | Phys_addr(Page::Section_cachable)
                | Phys_addr(local ? Page::Section_local : Page::Section_global);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu]:

#include <cstddef>
#include "types.h"
#include "cpu.h"
#include "paging.h"

unsigned long Bootstrap::load_addr;

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Phys_addr
Bootstrap::map_page_size_phys()
{ return Phys_addr(1) << map_page_order(); }

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Virt_addr
Bootstrap::map_page_size()
{ return Virt_addr(1) << map_page_order(); }

static inline void
Bootstrap::map_memory(void *pd, Virt_addr va, Phys_addr pa,
                      bool local)
{
  Phys_addr *const p = static_cast<Phys_addr *>(pd);
  p[cxx::int_value<Virt_addr>(va >> map_page_order())] = pt_entry(pa, local);
}


//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && arm_v5]:

static inline void Bootstrap::set_asid() {}
static inline void Bootstrap::set_ttbcr() {}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_1176_cache_alias_fix]:

PUBLIC static inline
void
Bootstrap::do_arm_1176_cache_alias_workaround() {}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu]:

#include "infinite_loop.h"
#include "kmem_space.h"
#include "mmu.h"
#include "globalconfig.h"

PUBLIC static inline ALWAYS_INLINE
void *
Bootstrap::kern_to_boot(void *a)
{
  return offset_cast<void *>(a, load_addr - Address{Mem_layout::Map_base});
}

extern "C" void bootstrap_main(unsigned long load_addr_delta_vs_link)
{
  if (load_addr_delta_vs_link)
    {
      Bootstrap::relocate(load_addr_delta_vs_link);

      // prevent compiler from reordering loads before applying the relocations
      Mem::barrier();

      bs_info.kernel_start_phys += load_addr_delta_vs_link;
      bs_info.kernel_end_phys += load_addr_delta_vs_link;
    }

  Bootstrap::load_addr = load_addr_delta_vs_link + bs_info.kernel_load_addr;

  Unsigned32 tbbr = cxx::int_value<Bootstrap::Phys_addr>(Bootstrap::init_paging())
                    | Page::Ttbr_bits;

  // Attention zone:
  // Only touch loader's own memory here until paging enabled.

  Bootstrap::do_arm_1176_cache_alias_workaround();
  Bootstrap::enable_paging(tbbr);

  // force to construct an absolute relocation because GCC may not do it.
  bs_info.entry();

  L4::infinite_loop();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu]:

#include "globalconfig.h"
#include "infinite_loop.h"

extern "C" void bootstrap_main()
{
  Bootstrap::switch_to_el1(); // NOP for cpu_virt config
  Bootstrap::init_node_data();

  // force to construct an absolute relocation because GCC may not do it.
  bs_info.entry();

  L4::infinite_loop();
}
