INTERFACE[sparc]:

#include "types.h"
#include "config.h"
#include <cassert>
#include "types.h"
#include "ptab_base.h"
#include "mem_unit.h"

typedef Ptab::Tupel< Ptab::Traits< Unsigned32, 24, 8, true>,
                     Ptab::Traits< Unsigned32, 18, 6, true>,
                     Ptab::Traits< Unsigned32, 12, 6, true> >::List Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;


class Paging {};

class Pte_ptr
{
public:
  typedef Mword Entry;

  enum
  {
    ET_invalid     = 0,           ///< Descriptor invalid
    ET_ptd         = 1,           ///< PT Descriptor is PTD
    ET_pte         = 2,           ///< PT Descriptor is PTE
    ET_mask        = 3,

    Ptp_mask       = 0xfffffffc,  ///< PTD: page table pointer
    Ppn_mask       = 0xffffff00,  ///< PTE: physical page number
    Ppn_addr_shift = 4,           ///< PTE/PTD: need to shift phys addr
    Cacheable      = 0x80,        ///< PTE: is cacheable
    Modified       = 0x40,        ///< PTE: modified
    Referenced     = 0x20,        ///< PTE: referenced
    Accperm_mask   = 0x1C,        ///< 3 bits for access permissions
    Accperm_shift  = 2,
    Et_mask        = 0x3,         ///< 2 bits to determine entry type
    Vfpa_mask      = 0xfffff000,  ///< Flush/Probe: virt. addr. mask
    Flushtype_mask = 0xf00,       ///< Flush/Probe: type

    Pdir_mask        = 0xFF,
    Pdir_shift       = 24,
    Ptab_mask        = 0x3F,
    Ptab_shift1      = 18,
    Ptab_shift2      = 12,
    Page_offset_mask = 0xFFF,

    Super_level    = 0,

    Acc_u_r_sv_r     = 0,
    Acc_u_rw_sv_rw   = 1,
    Acc_u_rx_sv_rx   = 2,
    Acc_u_rwx_sv_rwx = 3,
    Acc_u_x_sv_x     = 4,
    Acc_u_r_sv_rw    = 5,
    Acc_u_no_sv_rx   = 6,
    Acc_u_no_sv_rwx  = 7,
  };

  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level) : pte((Entry *)p), level(level) {}

  bool is_valid() const { return *pte & ET_mask; }

  void clear() { *pte = ET_invalid; }

  bool is_leaf() const
  {
    switch (level)
      {
	case 0:
	case 1: return (*pte & ET_mask) == ET_pte;
	default: return true;
      };
  }

  Mword next_level() const
  { return cxx::mask_lsb(*pte, 2) << Ppn_addr_shift; };

  void set_next_level(Mword phys)
  { write_now(pte, ((phys >> Ppn_addr_shift) & Ptp_mask) | ET_ptd); }

  Mword page_addr() const
  { return (*pte & Ppn_mask) << Ppn_addr_shift; }

  void set_pte(Address p, unsigned long attrs)
  { *pte = ((p >> Ppn_addr_shift) & Ppn_mask) | attrs | ET_pte; }

  void set_ptd(Address p)
  { *pte = ((p >> Ppn_addr_shift) & Ptp_mask) | ET_ptd; }

  Entry *pte;
  unsigned char level;
};

EXTENSION class Page
{
public:
  //typedef Unsigned32 Attribs;
  enum Attribs_enum
  {
    //Cache_mask   = 0x00000078,
    CACHEABLE    = 0x00000080,
    NONCACHEABLE = 0x00000000,
    BUFFERED     = 0x00000000, // Hmm...
  };
};

typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;
class Kpdir : public Pdir {};

// ------------------------------------------------------------------------
IMPLEMENTATION[sparc]:

#include "psr.h"
#include "lock_guard.h"
#include "cpu_lock.h"
#include "kip.h"

#include <cstdio>

PRIVATE inline
Mword
Pte_ptr::_attribs(Page::Attr attr) const
{
  Mword v = 0;

  if (attr.type == Page::Type::Normal())
    v |= Page::CACHEABLE;
  if (attr.type == Page::Type::Buffered())
    v |= Page::BUFFERED;
  if (attr.type == Page::Type::Uncached())
    v |= Page::NONCACHEABLE;

  Mword acc = 0;
  bool u(attr.rights & L4_fpage::Rights::U());
  bool r(attr.rights & L4_fpage::Rights::R());
  bool w(attr.rights & L4_fpage::Rights::W());
  bool x(attr.rights & L4_fpage::Rights::X());

  if (u)
    {
      if (w)
	acc = x ? Acc_u_rwx_sv_rwx : Acc_u_rw_sv_rw;
      else
	{
	  if (x)
            acc = r ? Acc_u_rx_sv_rx : Acc_u_x_sv_x;
          else
            acc = Acc_u_r_sv_rw;
	}
    }
  else
    acc = w ? Acc_u_no_sv_rwx : Acc_u_no_sv_rx;

  return v | (acc << Accperm_shift);
}

PUBLIC inline
bool
Pte_ptr::add_attribs(Page::Attr attr)
{
  Entry origpte = access_once(pte);
  auto p = (origpte & Accperm_mask) >> Accperm_shift;
  auto old = p;

  if (attr.rights & L4_fpage::Rights::W())
    {
      switch (p)
        {
	  case Acc_u_rx_sv_rx: p = Acc_u_rwx_sv_rwx; break;
	  case Acc_u_x_sv_x: p = Acc_u_rwx_sv_rwx; break;
	  case Acc_u_r_sv_rw: p = Acc_u_rw_sv_rw; break;
        }
    }

  if (attr.rights & L4_fpage::Rights::X())
    {
      switch (p)
        {
	  case Acc_u_rw_sv_rw: p = Acc_u_rwx_sv_rwx; break;
	  case Acc_u_r_sv_rw: p = Acc_u_rx_sv_rx; break;
        }
    }

  if (p != old)
    {
      write_now(pte, (origpte & ~Accperm_mask) | (p << Accperm_shift));
      return true;
    }

  return false;
}

PUBLIC inline NEEDS[Pte_ptr::_attribs]
void
Pte_ptr::create_page(Phys_mem_addr addr, Page::Attr attr)
{
  Mword p = ET_pte | _attribs(attr);
  p |= cxx::int_value<Phys_mem_addr>(addr) >> Ppn_addr_shift;
  write_now(pte, p);
}

PUBLIC inline
unsigned char Pte_ptr::page_order() const
{
  return Pdir::page_order_for_level(level);
}

/* this functions do nothing on SPARC architecture */
PUBLIC static inline
Address
Paging::canonize(Address addr)
{
  return addr;
}

PUBLIC static inline
Address
Paging::decanonize(Address addr)
{
  return addr;
}

IMPLEMENT inline
Mword PF::is_translation_error(Mword error)
{
  return !(error & 1 << 30) /* DSISR/SRR1 bit 1 */;
}

IMPLEMENT inline NEEDS["psr.h"]
Mword PF::is_usermode_error(Mword error)
{
  return 0 & error;//(error & Msr::Msr_pr);
}

IMPLEMENT inline
Mword PF::is_read_error(Mword error)
{
  return !(error & (1 << 25)) /* DSISR bit 6*/;
}

IMPLEMENT inline
Mword PF::addr_to_msgword0(Address pfa, Mword error)
{
  Mword a = pfa & ~7ul;
  if(is_translation_error(error))
    a |= 1;
  if(!is_read_error(error))
    a |= 2;

  // FIXME: flag instruction fetch faults with a |= 4
  return a;
}

PUBLIC static inline
bool
Pte_ptr::need_cache_write_back(bool current_pt)
{ return true; /*current_pt;*/ (void)current_pt; }

PUBLIC inline NEEDS["mem_unit.h"]
void
Pte_ptr::write_back_if(bool current_pt, Mword /*asid*/ = 0)
{
  (void)current_pt;
  //if (current_pt)
  //  Mem_unit::clean_dcache(pte);
}

PUBLIC static inline NEEDS["mem_unit.h"]
void
Pte_ptr::write_back(void *start, void *end)
{ (void)start; (void)end; }


//---------------------------------------------------------------------------

Pte_ptr::Entry context_table[16];
Mword kernel_srmmu_l1[256] __attribute__((aligned(0x400)));

PUBLIC static
void
Paging::set_ptbr(Address ptbr)
{
  Pte_ptr r(&context_table[0], 0);
  r.set_ptd(ptbr);
}

PUBLIC static
void
Paging::init()
{
  Address memstart, memend;
  memstart = memend = 0;
  printf("num descs %d\n", Kip::k()->num_mem_descs());
  for (auto const &md: Kip::k()->mem_descs_a())
    {
      printf("  [%lx - %lx type %x]\n", md.start(), md.end(), md.type());
      if ((memstart == 0) && md.type() == 1)
        {
          memstart = md.start();
          memend   = md.end();
          break;
        }
    }

  printf("Context table: %p - %p\n", context_table,
         context_table + (sizeof(context_table) / sizeof(context_table[0])));
  printf("Kernel PDir:   %p - %p\n", kernel_srmmu_l1,
         kernel_srmmu_l1 + (sizeof(kernel_srmmu_l1) / sizeof(kernel_srmmu_l1[0])));

  memset(context_table,   0, sizeof(context_table));
  memset(kernel_srmmu_l1, 0, sizeof(kernel_srmmu_l1));
  Mem_unit::context_table(Address(context_table));

  set_ptbr((Address)kernel_srmmu_l1);

  Mem_unit::context(0);

  /*
   * Map as many 16 MB chunks (1st level page table entries)
   * as possible.
   */
  enum { Superpage_size = 1 << Pte_ptr::Pdir_shift };
  Address virt = 0xf0000000;
  unsigned superpage_idx = virt >> Pte_ptr::Pdir_shift;
  while (memend - memstart + 1 >= Superpage_size)
    {
      printf("Chunk=%lx -> %lx\n", memstart, virt);
      assert(superpage_idx < sizeof(kernel_srmmu_l1) / sizeof(kernel_srmmu_l1[0]));

      unsigned long attr = Pte_ptr::Cacheable;
      attr |= Pte_ptr::Acc_u_no_sv_rwx << Pte_ptr::Accperm_shift;

      /* Assert proper zeros in PTE */
      assert((memstart & ~0xff000000) == 0);

      /* map phys mem starting from VA 0xF0000000 */
      Pte_ptr pte(&kernel_srmmu_l1[superpage_idx], 0);
      pte.set_pte(memstart, attr);

      Mem_layout::add_pmem(memstart, virt, Superpage_size);

      /* 1:1 mapping */
      Pte_ptr pte_identity(&kernel_srmmu_l1[memstart >> Pte_ptr::Pdir_shift], 0);
      pte_identity.set_pte(memstart, attr);

      memstart += Superpage_size;
      virt += Superpage_size;
      ++superpage_idx;
    }

  printf("Enabling paging...\n");
  Mem_unit::mmu_enable();
  printf("Paging enabled.\n");
}
