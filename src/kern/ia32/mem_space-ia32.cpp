INTERFACE [ia32 || ux || amd64]:

EXTENSION class Mem_space
{
public:
  typedef Pdir Dir_type;

  /** Return status of v_insert. */
  enum // Status
  {
    Insert_ok = 0,		///< Mapping was added successfully.
    Insert_warn_exists,		///< Mapping already existed
    Insert_warn_attrib_upgrade,	///< Mapping already existed, attribs upgrade
    Insert_err_nomem,		///< Couldn't alloc new page table
    Insert_err_exists		///< A mapping already exists at the target addr
  };

  /** Attribute masks for page mappings. */
  enum Page_attrib
  {
    Page_no_attribs = 0,
    /// Page is writable.
    Page_writable = Pt_entry::Writable,
    Page_cacheable = 0,
    /// Page is noncacheable.
    Page_noncacheable = Pt_entry::Noncacheable | Pt_entry::Write_through,
    /// it's a user page.
    Page_user_accessible = Pt_entry::User,
    /// Page has been referenced
    Page_referenced = Pt_entry::Referenced,
    /// Page is dirty
    Page_dirty = Pt_entry::Dirty,
    Page_references = Page_referenced | Page_dirty,
    /// A mask which contains all mask bits
    Page_all_attribs = Page_writable | Page_noncacheable |
                       Page_user_accessible | Page_referenced | Page_dirty,
  };

  // Mapping utilities

  enum				// Definitions for map_util
  {
    Need_insert_tlb_flush = 0,
    Map_page_size = Config::PAGE_SIZE,
    Page_shift = Config::PAGE_SHIFT,
    Whole_space = MWORD_BITS,
    Identity_map = 0,
  };


  void	page_map	(Address phys, Address virt,
                         Address size, Attr);

  void	page_unmap	(Address virt, Address size);

  void	page_protect	(Address virt, Address size,
                         unsigned page_attribs);

protected:
  // DATA
  Dir_type *_dir;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || ux || amd64]:

#include <cstring>
#include <cstdio>
#include "cpu.h"
#include "l4_types.h"
#include "mem_layout.h"
#include "paging.h"
#include "std_macros.h"




PUBLIC explicit inline
Mem_space::Mem_space(Ram_quota *q) : _quota(q), _dir(0) {}

PROTECTED inline
bool
Mem_space::initialize()
{
  void *b;
  if (EXPECT_FALSE(!(b = Kmem_alloc::allocator()
	  ->q_alloc(_quota, Config::PAGE_SHIFT))))
    return false;

  _dir = static_cast<Dir_type*>(b);
  _dir->clear(false);	// initialize to zero
  return true; // success
}

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type* pdir)
  : _quota(q), _dir(pdir)
{
  _kernel_space = this;
  _current.cpu(Cpu_number::boot_cpu()) = this;
}

PUBLIC static inline
bool
Mem_space::is_full_flush(L4_fpage::Rights rights)
{
  return (bool)(rights & L4_fpage::Rights::R());
}

PUBLIC inline NEEDS["cpu.h"]
static bool
Mem_space::has_superpages()
{
  return Cpu::have_superpages();
}


IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool = false)
{
  if (_current.current() == this)
    Mem_unit::tlb_flush();
}


IMPLEMENT inline
Mem_space *
Mem_space::current_mem_space(Cpu_number cpu) /// XXX: do not fix, deprecated, remove!
{
  return _current.cpu(cpu);
}

PUBLIC inline
bool
Mem_space::set_attributes(Virt_addr virt, Attr page_attribs)
{
  auto i = _dir->walk(virt);

  if (!i.is_valid())
    return false;

  i.set_attribs(page_attribs);
  return true;
}


PROTECTED inline
void
Mem_space::destroy()
{}

/**
 * Destructor.  Deletes the address space and unregisters it from
 * Space_index.
 */
PRIVATE
void
Mem_space::dir_shutdown()
{
  // free all page tables we have allocated for this address space
  // except the ones in kernel space which are always shared
  _dir->destroy(Virt_addr(0UL),
                Virt_addr(Mem_layout::User_max), 0, Pdir::Depth,
                Kmem_alloc::q_allocator(_quota));

  // free all unshared page table levels for the kernel space
  _dir->destroy(Virt_addr(Mem_layout::User_max + 1),
                Virt_addr(~0UL), 0, Pdir::Super_level,
                Kmem_alloc::q_allocator(_quota));

}

IMPLEMENT
Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                    Attr page_attribs)
{
  // insert page into page table

  // XXX should modify page table using compare-and-swap

  assert (cxx::is_zero(cxx::get_lsb(Phys_addr(phys), size)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  int level;
  for (level = 0; level <= Pdir::Depth; ++level)
    if (Page_order(Pdir::page_order_for_level(level)) <= size)
      break;

  auto i = _dir->walk(virt, level, false,
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
      page_protect(Virt_addr::val(virt), Address(1) << Page_order::val(size),
                   *i.pte & Page_all_attribs);

      return Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      page_map(Virt_addr::val(phys), Virt_addr::val(virt),
               Address(1) << Page_order::val(size), page_attribs);

      return Insert_ok;
    }

}

IMPLEMENT
void
Mem_space::v_set_access_flags(Vaddr virt, L4_fpage::Rights access_flags)
{
  auto i = _dir->walk(virt);

  if (EXPECT_FALSE(!i.is_valid()))
    return;

  unsigned page_attribs = 0;

  if (access_flags & L4_fpage::Rights::R())
    page_attribs |= Page_referenced;
  if (access_flags & L4_fpage::Rights::W())
    page_attribs |= Page_dirty;

  i.add_attribs(page_attribs);
}

/**
 * Simple page-table lookup.
 *
 * @param virt Virtual address.  This address does not need to be page-aligned.
 * @return Physical address corresponding to a.
 */
PUBLIC inline NEEDS ["paging.h"]
Address
Mem_space::virt_to_phys(Address virt) const
{
  return dir()->virt_to_phys(virt);
}

/**
 * Simple page-table lookup.
 *
 * @param virt Virtual address.  This address does not need to be page-aligned.
 * @return Physical address corresponding to a.
 */
PUBLIC inline NEEDS ["mem_layout.h"]
Address
Mem_space::pmem_to_phys(Address virt) const
{
  return Mem_layout::pmem_to_phys(virt);
}

/**
 * Simple page-table lookup.
 *
 * This method is similar to Space_context's virt_to_phys().
 * The difference is that this version handles Sigma0's
 * address space with a special case:  For Sigma0, we do not
 * actually consult the page table -- it is meaningless because we
 * create new mappings for Sigma0 transparently; instead, we return the
 * logically-correct result of physical address == virtual address.
 *
 * @param a Virtual address.  This address does not need to be page-aligned.
 * @return Physical address corresponding to a.
 */
PUBLIC inline
virtual Address
Mem_space::virt_to_phys_s0(void *a) const
{
  return dir()->virt_to_phys((Address)a);
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
Mem_space::v_delete(Vaddr virt, Page_order size, L4_fpage::Rights page_attribs)
{
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  auto i = _dir->walk(virt);

  if (EXPECT_FALSE (! i.is_valid()))
    return L4_fpage::Rights(0);

  assert (! (*i.pte & Pt_entry::global())); // Cannot unmap shared pages

  L4_fpage::Rights ret = i.access_flags();

  if (! (page_attribs & L4_fpage::Rights::R()))
    {
      // downgrade PDE (superpage) rights
      i.del_rights(page_attribs);
      page_protect(Virt_addr::val(virt), Address(1) << Page_order::val(size),
                   *i.pte & Page_all_attribs);
    }
  else
    {
      // delete PDE (superpage)
      i.clear();
      page_unmap(Virt_addr::val(virt), Address(1) << Page_order::val(size));
    }

  return ret;
}

/**
 * \brief Free all memory allocated for this Mem_space.
 * \pre Runs after the destructor!
 */
PUBLIC
Mem_space::~Mem_space()
{
  if (_dir)
    {
      dir_shutdown();
      Kmem_alloc::allocator()->q_free(_quota, Config::PAGE_SHIFT, _dir);
    }
}


// --------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include <cassert>
#include "l4_types.h"
#include "kmem.h"
#include "mem_unit.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "logdefs.h"
#include "paging.h"

#include <cstring>
#include "config.h"
#include "kmem.h"

IMPLEMENT inline NEEDS ["cpu.h", "kmem.h"]
void
Mem_space::make_current()
{
  Cpu::set_pdbr((Mem_layout::pmem_to_phys(_dir)));
  _current.cpu(current_cpu()) = this;
}

PUBLIC inline NEEDS ["kmem.h"]
Address
Mem_space::phys_dir()
{
  return Mem_layout::pmem_to_phys(_dir);
}

/*
 * The following functions are all no-ops on native ia32.
 * Pages appear in an address space when the corresponding PTE is made
 * ... unlike Fiasco-UX which needs these special tricks
 */

IMPLEMENT inline
void
Mem_space::page_map(Address, Address, Address, Attr)
{}

IMPLEMENT inline
void
Mem_space::page_protect(Address, Address, unsigned)
{}

IMPLEMENT inline
void
Mem_space::page_unmap(Address, Address)
{}

IMPLEMENT inline NEEDS["kmem.h", "logdefs.h"]
void
Mem_space::switchin_context(Mem_space *from)
{
  // FIXME: this optimization breaks SMP task deletion, an idle thread
  // may run on an already deleted page table
#if 0
  // never switch to kernel space (context of the idle thread)
  if (dir() == Kmem::dir())
    return;
#endif

  if (from != this)
    {
      CNT_ADDR_SPACE_SWITCH;
      make_current();
    }
}

PROTECTED inline
int
Mem_space::sync_kernel()
{
  return _dir->sync(Virt_addr(Mem_layout::User_max + 1), Kmem::dir(),
                    Virt_addr(Mem_layout::User_max + 1),
                    Virt_size(-(Mem_layout::User_max + 1)), Pdir::Super_level,
                    false,
                    Kmem_alloc::q_allocator(_quota));
}

// --------------------------------------------------------------------
IMPLEMENTATION [amd64]:

#include "cpu.h"

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{
  if (v & Page_number(Virt_addr(1UL << 48)))
    v = v | Page_number(Virt_addr(~0UL << 48));
  return v;
}

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  if (Cpu::cpus.cpu(Cpu_number::boot_cpu()).superpages())
    add_page_size(Page_order(21)); // 2MB

  if (Cpu::cpus.cpu(Cpu_number::boot_cpu()).ext_8000_0001_edx() & (1UL<<26))
    add_page_size(Page_order(30)); // 1GB
}

// --------------------------------------------------------------------
IMPLEMENTATION [ia32 || ux]:

#include "cpu.h"

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{ return v; }

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  if (Cpu::cpus.cpu(Cpu_number::boot_cpu()).superpages())
    add_page_size(Page_order(22)); // 4MB
}
