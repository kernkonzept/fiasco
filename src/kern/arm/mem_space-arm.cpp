INTERFACE [arm]:

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
    Need_insert_tlb_flush = 1,
    Map_page_size = Config::PAGE_SIZE,
    Page_shift = Config::PAGE_SHIFT,
    Whole_space = 32,
    Identity_map = 0,
  };

  Phys_mem_addr dir_phys() const { return _dir_phys; }

  static void kernel_space(Mem_space *);

private:
  // DATA
  Dir_type *_dir;
  Phys_mem_addr _dir_phys;

  static Kmem_slab_t<Dir_type, sizeof(Dir_type)> _dir_alloc;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

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

PUBLIC static inline
bool
Mem_space::is_full_flush(L4_fpage::Rights rights)
{
  return (bool)(rights & L4_fpage::Rights::R());
}

IMPLEMENT inline
Mem_space::Tlb_type
Mem_space::regular_tlb_type()
{
  return  Have_asids ? Tlb_per_cpu_asid : Tlb_per_cpu_global;
}

// Mapping utilities

IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool force = false)
{
  if (!Have_asids)
    Mem_unit::tlb_flush();
  else if (force && c_asid() != Mem_unit::Asid_invalid)
    {
      Mem_unit::tlb_flush(c_asid());
      tlb_mark_unused_if_non_current();
    }

  // else do nothing, we manage ASID local flushes in v_* already
  // Mem_unit::tlb_flush();
}


PUBLIC inline
bool
Mem_space::set_attributes(Virt_addr virt, Attr page_attribs,
                          bool writeback, Mword asid)
{
   auto i = _dir->walk(virt);

  if (!i.is_valid())
    return false;

  i.set_attribs(page_attribs);
  i.write_back_if(writeback, asid);
  return true;
}

IMPLEMENT inline
void Mem_space::kernel_space(Mem_space *_k_space)
{
  _kernel_space = _k_space;
}

IMPLEMENT
Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                    Attr page_attribs)
{
  bool const flush = _current.current() == this;
  assert (cxx::is_zero(cxx::get_lsb(Phys_addr(phys), size)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  int level;
  for (level = 0; level <= Pdir::Depth; ++level)
    if (Page_order(Pdir::page_order_for_level(level)) <= size)
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

      i.set_page(entry);
      i.write_back_if(flush, c_asid());
      return Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      i.write_back_if(flush, Mem_unit::Asid_invalid);
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


/** Simple page-table lookup.  This method is similar to
    virt_to_phys().  The difference is that this version handles
    Sigma0's address space with a special case: For Sigma0, we do not
    actually consult the page table -- it is meaningless because we
    create new mappings for Sigma0 transparently; instead, we return the
    logically-correct result of physical address == virtual address.
    @param a Virtual address.  This address does not need to be page-aligned.
    @return Physical address corresponding to a.
 */
PUBLIC inline
virtual Address
Mem_space::virt_to_phys_s0(void *a) const
{
  return virt_to_phys((Address)a);
}

IMPLEMENT
bool
Mem_space::v_lookup(Vaddr virt, Phys_addr *phys,
                    Page_order *order, Attr *page_attribs)
{
  auto i = _dir->walk(virt);
  if (order) *order = Page_order(i.page_order());

  if (!i.is_valid())
    return false;

  if (phys) *phys = Phys_addr(i.page_addr());
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

IMPLEMENT
L4_fpage::Rights
Mem_space::v_delete(Vaddr virt, Page_order size,
                    L4_fpage::Rights page_attribs)
{
  (void) size;
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));
  auto i = _dir->walk(virt);

  if (EXPECT_FALSE (! i.is_valid()))
    return L4_fpage::Rights(0);

  L4_fpage::Rights ret = i.access_flags();

  if (! (page_attribs & L4_fpage::Rights::R()))
    i.del_rights(page_attribs);
  else
    i.clear();

  i.write_back_if(_current.current() == this, c_asid());

  return ret;
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
      if (Virt_addr(Mem_layout::User_max) < Virt_addr(~0UL))
        _dir->destroy(Virt_addr(Mem_layout::User_max + 1),
                      Virt_addr(~0UL), 0, Pdir::Super_level,
                      Kmem_alloc::q_allocator(_quota));
      _dir_alloc.q_free(ram_quota(), _dir);
    }
}


PUBLIC inline
Mem_space::Mem_space(Ram_quota *q)
: _quota(q), _dir(0)
{}

PROTECTED inline NEEDS[<new>, "kmem_slab.h", "kmem.h"]
bool
Mem_space::initialize()
{
  _dir = _dir_alloc.q_new(ram_quota());
  if (!_dir)
    return false;

  _dir->clear(Pte_ptr::need_cache_write_back(false));
  _dir_phys = Phys_mem_addr(Kmem::kdir->virt_to_phys((Address)_dir));

  return true;
}

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type* pdir)
  : _quota(q), _dir (pdir)
{
  _current.cpu(Cpu_number::boot_cpu()) = this;
  _dir_phys = Phys_mem_addr(Kmem::kdir->virt_to_phys((Address)_dir));
}

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{ return v; }

IMPLEMENTATION [arm && !arm_lpae]:

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
Mem_space::v_set_access_flags(Vaddr, L4_fpage::Rights)
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v5]:

PUBLIC inline
unsigned long
Mem_space::c_asid() const
{ return 0; }

IMPLEMENT inline
void Mem_space::make_current()
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
INTERFACE [!(arm_v6 || arm_v7 || arm_v8)]:

EXTENSION class Mem_space
{
public:
  enum { Have_asids = 0 };
};

//----------------------------------------------------------------------------
INTERFACE [arm_v6 || arm_lpae]:

EXTENSION class Mem_space
{
  enum
  {
    Asid_base      = 0,
    Asid_bits      = 8,
    Asid_num       = 1UL << Asid_bits
  };
};

//----------------------------------------------------------------------------
INTERFACE [(arm_v7 || arm_v8) && !arm_lpae]:

EXTENSION class Mem_space
{
  enum
  {
    Asid_base      = 1,
    Asid_bits      = 8,
    Asid_num       = 1UL << Asid_bits
  };
};

//----------------------------------------------------------------------------
INTERFACE [arm_v6 || arm_v7 || arm_v8]:

#include "types.h"
#include "spin_lock.h"
#include <asid_alloc.h>

/*
  The ARM reference manual suggests to use the same address space id
  across multiple CPUs.
*/

EXTENSION class Mem_space
{
public:
  using Asid_alloc = Asid_alloc_t<Unsigned64, Asid_bits, Asid_base>;
  using Asid = Asid_alloc::Asid;
  using Asids = Asid_alloc::Asids_per_cpu;
  enum { Have_asids = 1 };

private:
  /// active/reserved ASID (per CPU)
  static Per_cpu<Asids> _asids;
  static Asid_alloc _asid_alloc;

  /// current ASID of mem_space, provided by _asid_alloc
  Asid _asid = Asid::Invalid;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v6 || arm_v7 || arm_v8]:
#include "cpu.h"
#include "cpu_lock.h"


PUBLIC inline NEEDS["atomic.h"]
unsigned long
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
  if (_asid_alloc.get_or_alloc_asid(&_asid))
    {
      Mem_unit::tlb_flush();
      Mem::dsb();
    }

  return _asid.asid();
};

DEFINE_PER_CPU Per_cpu<Mem_space::Asids> Mem_space::_asids;
Mem_space::Asid_alloc  Mem_space::_asid_alloc(&_asids);

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && !arm_pt_48]:

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  add_page_size(Page_order(21)); // 2MB
  add_page_size(Page_order(30)); // 1GB
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && arm_pt_48]:

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
