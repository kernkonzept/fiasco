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
INTERFACE[amd64 && ia32_pcid]:

#include "id_alloc.h"
#include "types.h"
#include "mem_unit.h"

EXTENSION class Mem_space
{
public:
  enum
  {
    Asid_num = (1 << 12) - 1,
    Asid_base = 1
  };

private:
  typedef Per_cpu_array<unsigned long> Asid_array;
  Asid_array _asid;

  struct Asid_ops
  {
    enum { Id_offset = Asid_base };

    static bool valid(Mem_space *o, Cpu_number cpu)
    { return o->_asid[cpu] != Mem_unit::Asid_invalid; }

    static unsigned long get_id(Mem_space *o, Cpu_number cpu)
    { return o->_asid[cpu]; }

    static bool can_replace(Mem_space *v, Cpu_number cpu)
    { return v != current_mem_space(cpu); }

    static void set_id(Mem_space *o, Cpu_number cpu, unsigned long id)
    {
      write_now(&o->_asid[cpu], id);
      Mem_unit::tlb_flush(id);
    }

    static void reset_id(Mem_space *o, Cpu_number cpu)
    { write_now(&o->_asid[cpu], (unsigned long)Mem_unit::Asid_invalid); }
  };

  struct Asid_alloc : Id_alloc<Unsigned16, Mem_space, Asid_ops>
  {
    Asid_alloc() : Id_alloc<Unsigned16, Mem_space, Asid_ops>(Asid_num) {}
  };

  static Per_cpu<Asid_alloc> _asid_alloc;
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

PROTECTED inline
bool
Mem_space::initialize()
{
  void *b;
  if (EXPECT_FALSE(!(b = Kmem_alloc::allocator()
	  ->q_alloc(_quota, Config::page_order()))))
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
      page_protect(cxx::int_value<Virt_addr>(virt), Address(1) << cxx::int_value<Page_order>(size),
                   *i.pte & Page_all_attribs);

      return Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      page_map(cxx::int_value<Virt_addr>(phys), cxx::int_value<Virt_addr>(virt),
               Address(1) << cxx::int_value<Page_order>(size), page_attribs);

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
      page_protect(cxx::int_value<Virt_addr>(virt), Address(1) << cxx::int_value<Page_order>(size),
                   *i.pte & Page_all_attribs);
    }
  else
    {
      // delete PDE (superpage)
      i.clear();
      page_unmap(cxx::int_value<Virt_addr>(virt), Address(1) << cxx::int_value<Page_order>(size));
    }

  return ret;
}

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

/**
 * \brief Free all memory allocated for this Mem_space.
 * \pre Runs after the destructor!
 */
PUBLIC
Mem_space::~Mem_space()
{
  reset_asid();
  if (_dir)
    {
      dir_shutdown();
      Kmem_alloc::allocator()->q_free(_quota, Config::page_order(), _dir);
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

IMPLEMENT inline NEEDS ["cpu.h", Mem_space::prepare_pt_switch,
                        Mem_space::switch_page_table]
void
Mem_space::make_current()
{
  prepare_pt_switch();
  switch_page_table();
  _current.cpu(current_cpu()) = this;
}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && cpu_local_map && ia32_pcid]:

PRIVATE inline NEEDS[Mem_space::cpu_val]
void
Mem_space::set_current_pcid()
{
  // [0]: CPU pdir pa + (if PCID: + bit 63 + ASID 0) -- not relevant here
  // [3]: CPU pdir pa + (if PCID: + bit 63 + ASID)
  Address pd_pa = cpu_val()[3];
  pd_pa &= ~0xfffUL;
  pd_pa |= asid();
  cpu_val()[3] = pd_pa;
}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && !ia32_pcid]:

PRIVATE inline
void
Mem_space::set_current_pcid()
{}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && cpu_local_map && intel_ia32_branch_barriers]:

PRIVATE inline NEEDS[Mem_space::cpu_val]
void
Mem_space::set_needs_ibpb()
{
  // set EXIT flags CPUE_EXIT_NEED_IBPB
  cpu_val()[2] |= 1;
}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && !intel_ia32_branch_barriers]:

PRIVATE inline
void
Mem_space::set_needs_ibpb()
{}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && cpu_local_map && kernel_isolation]:

PRIVATE inline NEEDS [Mem_space::set_current_pcid,
                      Mem_space::set_needs_ibpb]
void
Mem_space::switch_page_table()
{
  // We are currently running on the kernel page table. Prepare for switching
  // to the user page table on kernel exit.
  set_needs_ibpb();
  set_current_pcid();
}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && cpu_local_map && !kernel_isolation]:

PRIVATE inline NEEDS[Mem_space::cpu_val]
void
Mem_space::switch_page_table()
{
  // switch page table directly
  Cpu::set_pdbr(access_once(&cpu_val()[0]));
}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && cpu_local_map]:

PRIVATE static inline
Address *
Mem_space::cpu_val()
{ return reinterpret_cast<Address *>(Mem_layout::Kentry_cpu_page); }

PRIVATE inline NEEDS ["cpu.h", "kmem.h"]
void
Mem_space::prepare_pt_switch()
{
  Mword *pd = reinterpret_cast<Mword *>(Kmem::current_cpu_udir());
  Mword *d = (Mword *)_dir;
  auto *m = Kmem::pte_map();
  unsigned bit = 0;
  for (;;)
    {
      bit = m->ffs(bit);
      if (!bit)
        break;

      Mword n = d[bit - 1];
      pd[bit - 1] = n;
      if (n == 0)
        m->clear_bit(bit - 1);
      //printf("u: %u %lx\n", bit - 1, n);
      //LOG_MSG_3VAL(current(), "u", bit - 1, n, *reinterpret_cast<Mword *>(m));
    }
  asm volatile ("" : : : "memory");
}


PROTECTED inline
int
Mem_space::sync_kernel()
{
  return 0;
}

// --------------------------------------------------------------------
IMPLEMENTATION [(amd64 || ia32) && !cpu_local_map]:

PRIVATE inline
void
Mem_space::prepare_pt_switch()
{}

PRIVATE inline NEEDS["kmem.h"]
void
Mem_space::switch_page_table()
{
  // switch page table directly
  Cpu::set_pdbr(Mem_layout::pmem_to_phys(_dir));
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

//----------------------------------------------------------------------------
IMPLEMENTATION [!ia32_pcid]:

PUBLIC explicit inline
Mem_space::Mem_space(Ram_quota *q) : _quota(q), _dir(0) {}

IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool = false)
{
  if (_current.current() == this)
    Mem_unit::tlb_flush();
}

PRIVATE inline
void
Mem_space::reset_asid() {}

//----------------------------------------------------------------------------
IMPLEMENTATION [amd64 && ia32_pcid]:

DEFINE_PER_CPU Per_cpu<Mem_space::Asid_alloc> Mem_space::_asid_alloc;

PUBLIC explicit inline NEEDS[Mem_space::asid]
Mem_space::Mem_space(Ram_quota *q) : _quota(q), _dir(0)
{
  asid(Mem_unit::Asid_invalid);
}

PRIVATE inline
void
Mem_space::asid(unsigned long a)
{
  for (Asid_array::iterator i = _asid.begin(); i != _asid.end(); ++i)
    *i = a;
}

PUBLIC inline
unsigned long
Mem_space::c_asid() const
{ return _asid[current_cpu()]; }

PRIVATE inline
unsigned long
Mem_space::asid()
{
  Cpu_number cpu = current_cpu();
  return _asid_alloc.cpu(cpu).alloc(this, cpu);
};

PRIVATE inline
void
Mem_space::reset_asid()
{
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    _asid_alloc.cpu(i).free(this, i);
}

IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool = false)
{
  auto asid = c_asid();
  if (asid != Mem_unit::Asid_invalid)
    Mem_unit::tlb_flush(asid);
}
