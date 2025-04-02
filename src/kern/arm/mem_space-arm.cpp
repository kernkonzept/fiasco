INTERFACE [arm && mmu]:

#include "auto_quota.h"
#include "kmem.h"		// for "_unused_*" virtual memory regions
#include "kmem_slab.h"
#include "member_offs.h"
#include "paging.h"
#include "types.h"
#include "ram_quota.h"
#include "config.h"

EXTENSION class Mem_space
{
  friend class Jdb;
  friend class Mem_space_test;

public:
  typedef Pdir Dir_type;

  /** Return status of v_insert. */
  enum // Status
  {
    Insert_ok = 0,		///< Mapping was added successfully.
    Insert_err_exists, ///< A mapping already exists at the target addr
    Insert_warn_attrib_upgrade,	///< Mapping already existed, attribs upgrade
    Insert_err_nomem,  ///< Couldn't alloc new page table
    Insert_warn_exists,		///< Mapping already existed

  };

  // Mapping utilities
  enum				// Definitions for map_util
  {
    // TLB never holds any translation table entry that generates a Translation
    // fault or an Access Flag fault.
    Need_insert_tlb_flush = 0,
    // TLB entry needs to be explicitly invalidated before it can be used.
    Need_upgrade_tlb_flush = 1,
    Map_page_size = Config::PAGE_SIZE,
    Page_shift = Config::PAGE_SHIFT,
    Whole_space = 32,
    Identity_map = 0,
  };

  Phys_mem_addr dir_phys() const { return _dir_phys; }

  static void kernel_space(Mem_space *);

  /**
   * Return maximum supported user space address.
   *
   * The page tables always provide up to Mem_layout::User_max bits of virtual
   * address space. But at least on arm64 cpu_virt the HW supported stage1
   * output size (maximum IPA size) is additionally constrained by the
   * available physical address size of the MMU.
   */
  static Address user_max();

private:
  // DATA
  Dir_type *_dir;
  Phys_mem_addr _dir_phys;

  static Kmem_slab_t<Dir_type, sizeof(Dir_type)> _dir_alloc;
};

//---------------------------------------------------------------------------
INTERFACE [arm && mpu]:

#include "auto_quota.h"
#include "config.h"
#include "context_base.h"
#include "kmem.h"
#include "kmem_slab.h"
#include "lock_guard.h"
#include "member_offs.h"
#include "paging.h"
#include "per_cpu_data.h"
#include "ram_quota.h"
#include "spin_lock.h"
#include "types.h"

EXTENSION class Mem_space
{
  friend class Jdb;

  enum
  {
    Debug_failures = Config::Warn_level > 0,
    Debug_allocation = 0,
    Debug_free = 0,
  };

public:
  typedef Pdir Dir_type;

  /** Return status of v_insert. */
  enum // Status
  {
    Insert_ok = 0,              ///< Mapping was added successfully.
    Insert_err_exists,          ///< A mapping already exists at the target addr
    Insert_warn_attrib_upgrade, ///< Mapping already existed, attribs upgrade
    Insert_err_nomem,           ///< Couldn't alloc new page table
    Insert_warn_exists,         ///< Mapping already existed
  };

  // Definitions for map_util
  enum
  {
    Need_insert_tlb_flush = 1,          ///< Need to reload MPU on remote CPUs
    Need_upgrade_tlb_flush = 1,         ///< Need to reload MPU on remote CPUs
    Map_page_size = Config::PAGE_SIZE,
    Page_shift = Config::PAGE_SHIFT,
    Whole_space = 32,
    Identity_map = 1,
  };

  static void kernel_space(Mem_space *);

  static Address user_max();

  /**
   * Load mask of active ku_mem regions in current context.
   *
   * Must be called whenever the Context or Mem_space is switched or when
   * ku_mem of the active Context is mapped.
   */
  inline void load_ku_mem_regions() const
  { Context_base::load_ku_mem_regions(_ku_mem_regions); }

private:
  inline void ku_mem_added(Unsigned32 touched)
  { _ku_mem_regions |= touched; }

  /**
   * Pointer to Mpu_regions object of this Mem_space.
   *
   * Usually this points to the adjacent _regions object in regular user space
   * tasks. For the Kernel_task it will instad point to `Kmem::kdir`, though.
   */
  Dir_type *_dir;
  Dir_type _regions;
  Unsigned32 _ku_mem_regions = 0;
  Spin_lock<> _lock;
};

//---------------------------------------------------------------------------
INTERFACE [arm && mpu && mp]:

EXTENSION class Mem_space
{
private:
  /**
   * Update counter of regions.
   *
   * Incremented on every update of `_dir`. Used in conjunction with
   * `_dir_update_state` to check if the hardware state is in sync. Protected
   * by `_lock`.
   */
  Unsigned32 _dir_update_count;

  /**
   * Per-CPU update counter state of currently active Mem_space.
   *
   * Protected by `cpu_lock`.
   */
  static Per_cpu<Unsigned32> _dir_update_state;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "mem_layout.h"
#include "mem_unit.h"

PUBLIC static inline
bool
Mem_space::is_full_flush(L4_fpage::Rights rights)
{
  return static_cast<bool>(rights & L4_fpage::Rights::R());
}

IMPLEMENT inline
Mem_space::Tlb_type
Mem_space::regular_tlb_type()
{
  return  Have_asids ? Tlb_per_cpu_asid : Tlb_per_cpu_global;
}

IMPLEMENT_DEFAULT inline NEEDS["mem_layout.h"]
Address
Mem_space::user_max()
{ return Mem_layout::User_max; }

IMPLEMENT inline NEEDS["mem_unit.h", Mem_space::sync_mpu_state]
void
Mem_space::tlb_flush_current_cpu()
{
  sync_mpu_state();

  if constexpr (!Have_asids)
    Mem_unit::tlb_flush();
  else if (c_asid() != Mem_unit::Asid_invalid)
    {
      Mem_unit::tlb_flush(c_asid());
      tlb_mark_unused_if_non_current();
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu]:

#include <cassert>
#include <cstring>
#include <new>

#include "atomic.h"
#include "config.h"
#include "globals.h"
#include "l4_types.h"
#include "logdefs.h"
#include "panic.h"
#include "paging.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "mem_unit.h"

Kmem_slab_t<Mem_space::Dir_type,
            sizeof(Mem_space::Dir_type)> Mem_space::_dir_alloc;

// Mapping utilities

IMPLEMENT inline
void Mem_space::kernel_space(Mem_space *_k_space)
{
  _kernel_space = _k_space;
}

IMPLEMENT
Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order order,
                    Attr page_attribs, bool)
{
  bool const flush = _current.current() == this;
  assert (cxx::is_zero(cxx::get_lsb(Phys_addr(phys), order)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  int level;
  for (level = 0; level <= Pdir::Depth; ++level)
    if (Page_order(Pdir::page_order_for_level(level)) <= order)
      break;

  auto i = _dir->walk(virt, level, Pte_ptr::need_cache_write_back(flush),
                      Kmem_alloc::q_allocator(_quota));

  if (EXPECT_FALSE(!i.is_valid() && i.level != level))
    return Insert_err_nomem;

  if (EXPECT_FALSE(i.is_valid()
                   && (i.level != level || Phys_addr(i.page_addr()) != phys)))
    return Insert_err_exists;

  bool const valid = i.is_valid();
  if (valid)
    page_attribs.rights |= i.attribs().rights;

  auto entry = i.make_page(phys, page_attribs);

  if (valid)
    {
      if (EXPECT_FALSE(i.entry() == entry))
        return Insert_warn_exists;

      // Does the attribute upgrade require a break-before-make sequence?
      if (!Page::is_attribs_upgrade_safe(i.attribs(), page_attribs))
        {
          // The following is only a complete break-before-make sequence in
          // a multi-core system if broadcast TLB flush instructions are in use,
          // thus no IPI based TLB shootdown is necessary! This should never be
          // a problem, because the need for a break-before-make sequence for
          // certain attribute upgrades only exists since ARMv7, which coincides
          // with our use of broadcast TLB flush instructions.
          assert(!Need_xcpu_tlb_flush);

          // Replace the old entry with an invalid entry.
          i.clear();
          // Ensure the invalid entry is visible to all CPUs.
          i.write_back_if(flush, c_asid());
          // Now we can safely write the new entry.
        }

      i.set_page(entry);
      i.write_back_if(flush, c_asid());
      return Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      i.write_back_if(flush, Mem_unit::Asid_invalid);
      // Because the entry was invalid before, no TLB maintenance is necessary.
      // We still have to make sure that the MMU sees the new entry before
      // potentially triggering a page walk on it. Otherwise we might get
      // unexpected data-/prefetch-aborts...
      Mem::dsb();
      return Insert_ok;
    }
}


/**
 * Simple page-table lookup.
 *
 * @param virt Virtual address.  This address does not need to be page-aligned.
 * @return Physical address corresponding to a.
 */
PUBLIC inline
Address
Mem_space::virt_to_phys(Address virt) const
{
  return dir()->virt_to_phys(virt);
}


IMPLEMENT
bool
Mem_space::v_lookup(Vaddr virt, Phys_addr *phys,
                    Page_order *order, Attr *page_attribs)
{
  auto i = _dir->walk(virt);
  if (order)
    *order = Page_order(i.page_order());

  if (!i.is_valid())
    return false;

  if (phys)
    *phys = Phys_addr(i.page_addr());
  if (page_attribs)
    *page_attribs = i.attribs();

  return true;
}

IMPLEMENT
Page::Flags
Mem_space::v_delete(Vaddr virt, [[maybe_unused]] Page_order order,
                    Page::Rights rights)
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto pte = _dir->walk(virt);

  if (EXPECT_FALSE(!pte.is_valid()))
    return Page::Flags::None();

  Page::Flags flags = pte.access_flags();

  if (!(rights & Page::Rights::R()))
    pte.del_rights(rights);
  else
    pte.clear();

  pte.write_back_if(_current.current() == this, c_asid());

  return flags;
}



PUBLIC
Mem_space::~Mem_space()
{
  if (_dir)
    {

      // free all page tables we have allocated for this address space
      // except the ones in kernel space which are always shared
      _dir->destroy(Virt_addr(0UL),
                    Virt_addr(Mem_layout::User_max), 0, Pdir::Depth,
                    Kmem_alloc::q_allocator(_quota));
      // free all unshared page table levels for the kernel space
      if constexpr (Virt_addr(Mem_layout::User_max) < Virt_addr(Pdir::Max_addr))
        _dir->destroy(Virt_addr(Mem_layout::User_max + 1),
                      Virt_addr(Pdir::Max_addr), 0, Pdir::Super_level,
                      Kmem_alloc::q_allocator(_quota));
      _dir_alloc.q_free(ram_quota(), _dir);
    }
}


PUBLIC inline
Mem_space::Mem_space(Ram_quota *q)
: _quota(q), _dir(nullptr)
{}

PROTECTED inline NEEDS[<new>, "kmem_slab.h", "kmem.h"]
bool
Mem_space::initialize()
{
  _dir = _dir_alloc.q_new(ram_quota());
  if (!_dir)
    return false;

  _dir->clear(Pte_ptr::need_cache_write_back(false));
  _dir_phys =
    Phys_mem_addr(Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(_dir)));

  return true;
}

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type* pdir)
  : _quota(q), _dir (pdir)
{
  _current.cpu(Cpu_number::boot_cpu()) = this;
  _dir_phys =
    Phys_mem_addr(Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(_dir)));
}

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{ return v; }

PRIVATE inline
void
Mem_space::sync_mpu_state()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu]:

#include <cassert>
#include <cstring>

#include "arithmetic.h"
#include "atomic.h"
#include "config.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "l4_types.h"
#include "logdefs.h"
#include "mem_unit.h"
#include "paging.h"
#include "paging_bits.h"

IMPLEMENT_OVERRIDE static
void
Mem_space::reload_current()
{
  auto *mem_space = current_mem_space(current_cpu());
  auto guard = lock_guard(mem_space->_lock);
  Mpu::update(*mem_space->_dir);
  mem_space->mpu_state_mark_in_sync();
  mem_space->load_ku_mem_regions();
}

IMPLEMENT inline
void Mem_space::kernel_space(Mem_space *_k_space)
{
  _kernel_space = _k_space;
}

IMPLEMENT
Mem_space::Status
Mem_space::v_insert([[maybe_unused]] Phys_addr phys,
                    Vaddr virt, Page_order order,
                    Attr page_attribs, bool ku_mem)
{
  assert (phys == virt);
  assert (cxx::is_zero(cxx::get_lsb(Phys_addr(phys), order)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));
  Mword start = Vaddr::val(virt);
  Mword end = Vaddr::val(virt) + (1UL << Page_order::val(order)) - 1U;
  Mpu_region_attr attr = Mpu_region_attr::make_attr(page_attribs.rights,
                                                    page_attribs.type,
                                                    !ku_mem);
  Mem_space::Status ret = Insert_ok;

  auto guard = lock_guard(_lock);

  // This check must be done without interrupts to prevent thread migration.
  // The MPU registers must only be updated if the Mem_space is currently
  // active and the register state is in sync.
  bool const writeback = _current.current() == this && mpu_state_in_sync();

  auto touched = _dir->add(start, end, attr);
  if (EXPECT_FALSE(!touched))
    {
      if (touched.error() == Mpu_regions_update::Error_no_mem)
        {
          WARN("Mem_space::v_insert(%p): no region available for ["
                 L4_MWORD_FMT ":" L4_MWORD_FMT "]\n",
               this, start, end);
          if constexpr (Debug_failures)
            _dir->dump();
          return Insert_err_nomem;
        }

      mpu_state_mark_dirty();

      if (touched.error() == Mpu_regions_update::Error_collision)
        {
          // This cannot happen for ku_mem because this is allocated from
          // kernel heap.
          assert(!ku_mem);

          // Punch hole with new attributes! Will probably occupy two new
          // regions in the end! There is still some room for improvement here:
          //  * make the delete-and-add an atomic transaction
          //  * do nothing in case the attributes do not change
          Mpu_region_attr old_attr;
          touched = _dir->del(start, end, &old_attr);
          attr.add_rights(old_attr.rights());
          auto added = _dir->add(start, end, attr);
          if (EXPECT_FALSE(!added))
            {
              WARN("Mem_space::v_insert(%p): dropped [" L4_MWORD_FMT ":"
                   L4_MWORD_FMT "]\n", this, start, end);
              if constexpr (Debug_failures)
                _dir->dump();
              ret = Insert_err_nomem;
            }
          else
            {
              touched |= added;
              if (attr == old_attr)
                ret = Insert_warn_exists;
              else
                ret = Insert_warn_attrib_upgrade;
            }
        }
    }

  if (ku_mem)
    ku_mem_added(*touched.value().raw());

  if (writeback)
    {
      Mpu::sync(*_dir, touched.value());
      mpu_state_mark_in_sync();
      load_ku_mem_regions();
    }

  if constexpr (Debug_allocation)
    {
      printf("Mem_space::v_insert(%p, " L4_MWORD_FMT "/%u): ", this, start,
             Page_order::val(order));
      _dir->dump();
    }

  return ret;
}

PUBLIC inline
Address
Mem_space::virt_to_phys(Address virt) const
{
  return virt;
}

IMPLEMENT
bool
Mem_space::v_lookup(Vaddr virt, Phys_addr *phys,
                    Page_order *order, Attr *page_attribs)
{
  auto guard = lock_guard(_lock);

  Mword v(Vaddr::val(virt));
  auto r = _dir->find(v);
  if (!r)
    {
      if (order)
        {
          // Calculate the largest possible order that covers the gap to make
          // iterating over the vast address space during unmap() viable.
          Mword gap_end(0);
          auto n = _dir->find_next(v);
          if (n)
            gap_end = n->start();
          Mword gap_size = gap_end - v;
          unsigned gap_order = gap_size ? cxx::log2u(gap_size) : MWORD_BITS - 1;
          unsigned max_order = v ? __builtin_ctzl(v) : MWORD_BITS - 1;
          *order = Page_order(min(gap_order, max_order));
        }
      return false;
    }

  // We always need to report the smallest order. The disabled code below that
  // tries to report the order more truthfully breaks mapdb.
  if (order)
    *order = Page_order(Config::PAGE_SHIFT);
    //{
    //  bool super_aligned = Super_pg::aligned(r->start() | (r->end() + 1U));
    //  *order = super_aligned ? Page_order(Config::SUPERPAGE_SHIFT)
    //                         : Page_order(Config::PAGE_SHIFT);
    //}

  if (phys)
    *phys = virt;
  if (page_attribs)
    *page_attribs = Attr(r->attr().rights(), r->attr().type(),
                         Page::Kern::None(), Page::Flags::None());

  return true;
}

IMPLEMENT
Page::Flags
Mem_space::v_delete(Vaddr virt, Page_order order,
                    Page::Rights rights)
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  Mword start = Vaddr::val(virt);
  Mword end = Vaddr::val(virt) + (1UL << Page_order::val(order)) - 1U;
  Mpu_region_attr attr;

  auto guard = lock_guard(_lock);

  // This check must be done without interrupts to prevent thread migration.
  // The MPU registers must only be updated if the Mem_space is currently
  // active and the register state is in sync.
  bool const writeback = _current.current() == this && mpu_state_in_sync();

  auto touched = _dir->del(start, end, &attr);
  assert(touched); // Deleting regions must never fail.
  if (EXPECT_FALSE(touched.value().is_empty()))
    return Page::Flags::None();

  mpu_state_mark_dirty();

  // Re-add if page stays readable. If the attributes are compatible the
  // regions will be joined again.
  if (!(rights & Page::Rights::R()))
    {
      Mpu_region_attr new_attr = attr;
      new_attr.del_rights(rights);
      auto added = _dir->add(start, end, new_attr);
      if (EXPECT_FALSE(!added))
        WARN("Mem_space::v_delete(%p): dropped [" L4_MWORD_FMT ":" L4_MWORD_FMT "]\n",
             this, start, end);
      else
        touched |= added;
    }

  if (writeback)
    {
      Mpu::sync(*_dir, touched.value());
      mpu_state_mark_in_sync();
      load_ku_mem_regions();
    }

  if constexpr (Debug_free)
    {
      printf("Mem_space::v_delete(%p, " L4_MWORD_FMT "/%u): ", this, start,
             Page_order::val(order));
      _dir->dump();
    }

  return Page::Flags::None();
}

PUBLIC inline
Mem_space::Mem_space(Ram_quota *q)
: _quota(q), _dir(&_regions),
  _regions(static_cast<Mpu_regions const &>(*Kmem::kdir))
{}

PROTECTED inline
bool
Mem_space::initialize()
{ return true; }

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type* pdir)
  : _quota(q), _dir(pdir)
{}

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{ return v; }

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu && mp]:

DEFINE_PER_CPU Per_cpu<Unsigned32> Mem_space::_dir_update_state;

PRIVATE inline NEEDS[Mem_space::mpu_state_in_sync,
                     Mem_space::mpu_state_mark_in_sync]
void
Mem_space::sync_mpu_state()
{
  // If the Mem_space was manipulated on a different CPU and it is currently
  // active here, we have to reload the MPU as a whole.
  if (_current.current() == this)
    {
      auto guard = lock_guard(_lock);
      if (EXPECT_TRUE(!mpu_state_in_sync()))
        {
          Mpu::update(*_dir);
          mpu_state_mark_in_sync();
          load_ku_mem_regions();
        }
    }
}

PRIVATE inline
bool
Mem_space::mpu_state_in_sync()
{
  return _dir_update_count == _dir_update_state.current();
}

PRIVATE inline
void
Mem_space::mpu_state_mark_in_sync()
{
  _dir_update_state.current() = _dir_update_count;
}

PRIVATE inline
void
Mem_space::mpu_state_mark_dirty()
{
  _dir_update_count++;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu && !mp]:

PRIVATE inline
void
Mem_space::sync_mpu_state()
{}

PRIVATE inline
bool
Mem_space::mpu_state_in_sync()
{
  // On UP sytems, the hardware state is always in sync to the currently active
  // Mem_space.
  return true;
}

PRIVATE inline
void
Mem_space::mpu_state_mark_in_sync()
{}

PRIVATE inline
void
Mem_space::mpu_state_mark_dirty()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && !arm_lpae]:

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  add_page_size(Page_order(20)); // 1MB
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v5 || arm_v6 || arm_v7 || arm_v8]:

IMPLEMENT inline
void
Mem_space::v_add_access_flags(Vaddr, Page::Flags)
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v5]:

PUBLIC inline
unsigned long FIASCO_PURE
Mem_space::c_asid() const
{ return 0; }

IMPLEMENT inline
void Mem_space::make_current(Switchin_flags)
{
  _current.current() = this;
  Mem_unit::flush_vcache();
  asm volatile (
      "mcr p15, 0, r0, c8, c7, 0 \n" // TLBIALL
      "mcr p15, 0, %0, c2, c0    \n" // TTBR0

      "mrc p15, 0, r1, c2, c0    \n"
      "mov r1, r1                \n"
      "sub pc, pc, #4            \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys))
      : "r1");
}

//----------------------------------------------------------------------------
INTERFACE [!(arm_v6 || arm_v7 || arm_v8) || explicit_asid]:

EXTENSION class Mem_space
{
public:
  static constexpr bool Have_asids = false;
};

//----------------------------------------------------------------------------
INTERFACE [arm_v6 || arm_lpae || mpu]:

EXTENSION class Mem_space
{
  enum
  {
    Asid_base      = 0,
  };
};

//----------------------------------------------------------------------------
INTERFACE [(arm_v7 || arm_v8) && !(arm_lpae || mpu)]:

EXTENSION class Mem_space
{
  enum
  {
    // ASID 0 is reserved for synchronizing the update of ASID and translation
    // table base address, which is necessary when using the short-descriptor
    // translation table format, because with this format different registers
    // hold these two values, so an atomic update is not possible (see
    // "Synchronization of changes of ASID and TTBR" in ARM DDI 0487H.a).
    Asid_base      = 1,
  };
};

//----------------------------------------------------------------------------
INTERFACE [(arm_v6 || arm_v7 || arm_v8) && !explicit_asid]:

#include "types.h"
#include "spin_lock.h"
#include <asid_alloc.h>
#include "global_data.h"

/*
  The ARM reference manual suggests to use the same address space id
  across multiple CPUs.
*/

EXTENSION class Mem_space
{
public:
  using Asid_alloc = Asid_alloc_t<Unsigned64, Mem_unit::Asid_bits, Asid_base>;
  using Asid = Asid_alloc::Asid;
  using Asids = Asid_alloc::Asids_per_cpu;
  static constexpr bool Have_asids = true;

private:
  /// active/reserved ASID (per CPU)
  static Per_cpu<Asids> _asids;
  static Global_data<Asid_alloc> _asid_alloc;

  /// current ASID of mem_space, provided by _asid_alloc
  Asid _asid = Asid::Invalid;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v6 || arm_v7 || arm_v8) && !explicit_asid]:
#include "cpu.h"
#include "cpu_lock.h"


PUBLIC inline NEEDS["atomic.h"]
unsigned long FIASCO_PURE
Mem_space::c_asid() const
{
  Asid asid = atomic_load(&_asid);

  if (EXPECT_TRUE(asid.is_valid()))
    return asid.asid();
  else
    return Mem_unit::Asid_invalid;
}

PUBLIC inline NEEDS[<asid_alloc.h>]
unsigned long
Mem_space::asid()
{
  if (_asid_alloc->get_or_alloc_asid(&_asid))
    {
      Mem_unit::tlb_flush();
      Mem::dsb();
    }

  return _asid.asid();
};

DEFINE_PER_CPU Per_cpu<Mem_space::Asids> Mem_space::_asids;

DEFINE_GLOBAL_CONSTINIT
Global_data<Mem_space::Asid_alloc> Mem_space::_asid_alloc(&_asids);

//----------------------------------------------------------------------------
INTERFACE [(arm_v6 || arm_v7 || arm_v8) && explicit_asid]:

#include "types.h"

EXTENSION class Mem_space
{
  unsigned _asid = 0;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v6 || arm_v7 || arm_v8) && explicit_asid]:

#include "cpu.h"

PUBLIC inline NEEDS["atomic.h"]
unsigned long FIASCO_PURE
Mem_space::c_asid() const
{
  return atomic_load(&_asid);
}

PUBLIC inline
unsigned long
Mem_space::asid()
{
  return _asid;
};

PUBLIC inline NEEDS["atomic.h"]
void
Mem_space::asid(unsigned asid)
{
  atomic_store(&_asid, asid);
};

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && arm_lpae && !arm_pt_48]:

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  add_page_size(Page_order(21)); // 2MB
  add_page_size(Page_order(30)); // 1GB
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && arm_lpae && arm_pt_48]:

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  add_page_size(Page_order(21)); // 2MB
  add_page_size(Page_order(30)); // 1GB
  add_page_size(Page_order(39)); // 512GB
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && need_xcpu_tlb_flush]:

IMPLEMENT inline
void
Mem_space::sync_write_tlb_active_on_cpu()
{
  // Ensure that the write to _tlb_active_on_cpu (store) is visible to all other
  // CPUs, before any page table entry of this memory space is accessed on the
  // current CPU, thus potentially cached in the TLB. Or rather before returning
  // to user space, because only page table entries "not from a translation
  // regime for an Exception level that is lower than the current Exception
  // level can be allocated to a TLB at any time" (see ARM DDI 0487 H.a
  // D5-4907).
  //
  // However, the only way to ensure a globally visible order between an
  // ordinary store (write to _tlb_active_on_cpu) and an access by the page
  // table walker seems to be the DSB instruction. Since a DSB instruction is
  // not necessarily on the return path to user mode, we need to execute one
  // here.
  Mem::dsb();
}

IMPLEMENT inline
void
Mem_space::sync_read_tlb_active_on_cpu()
{
  // Ensure that all changes to page tables (store) are visible to all other
  // CPUs, before accessing _tlb_active_on_cpu (load) on the current CPU. This
  // has to be DSB, because the page table walker is considered to be a separate
  // observer, for which a store to a page table "is only guaranteed to be
  // observable after the execution of a DSB instruction by the PE that executed
  // the store" (see ARM DDI 0487 H.a D5-4927)
  Mem::dsb();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu]:

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  add_page_size(Page_order(21)); // 2MB
}
