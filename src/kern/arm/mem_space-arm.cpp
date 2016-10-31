INTERFACE [arm]:

#include "auto_quota.h"
#include "kmem.h"		// for "_unused_*" virtual memory regions
#include "kmem_slab.h"
#include "member_offs.h"
#include "paging.h"
#include "types.h"
#include "ram_quota.h"

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
#include "panic.h"
#include "paging.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "kmem_space.h"
#include "mem_unit.h"

Kmem_slab_t<Mem_space::Dir_type,
            sizeof(Mem_space::Dir_type)> Mem_space::_dir_alloc;

PUBLIC static inline
bool
Mem_space::is_full_flush(L4_fpage::Rights rights)
{
  return rights & L4_fpage::Rights::R();
}

// Mapping utilities

IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool force = false)
{
  if (!Have_asids)
    Mem_unit::tlb_flush();
  else if (force && c_asid() != Mem_unit::Asid_invalid)
    Mem_unit::tlb_flush(c_asid());

  // else do nothing, we manage ASID local flushes in v_* already
  // Mem_unit::tlb_flush();
}

PUBLIC static inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush_spaces(bool all, Mem_space *s1, Mem_space *s2)
{
  if (all || !Have_asids)
    Mem_unit::tlb_flush();
  else
    {
      if (s1)
	s1->tlb_flush(true);
      if (s2)
	s2->tlb_flush(true);
    }
}


IMPLEMENT inline
Mem_space *Mem_space::current_mem_space(Cpu_number cpu)
{
  return _current.cpu(cpu);
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

IMPLEMENT inline NEEDS ["kmem.h", Mem_space::c_asid]
void Mem_space::switchin_context(Mem_space *from)
{
#if 0
  // never switch to kernel space (context of the idle thread)
  if (this == kernel_space())
    return;
#endif

  if (from != this)
    make_current();
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
  assert (cxx::get_lsb(Phys_addr(phys), size) == 0);
  assert (cxx::get_lsb(Virt_addr(virt), size) == 0);

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


/** Simple page-table lookup.  This method is similar to Mem_space's 
    lookup().  The difference is that this version handles 
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
  assert (cxx::get_lsb(Virt_addr(virt), size) == 0);
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


/** Constructor.  Creates a new address space and registers it with
  * Space_index.
  *
  * Registration may fail (if a task with the given number already
  * exists, or if another thread creates an address space for the same
  * task number concurrently).  In this case, the newly-created
  * address space should be deleted again.
  */
PUBLIC inline NEEDS[Mem_space::asid]
Mem_space::Mem_space(Ram_quota *q)
: _quota(q), _dir(0)
{
  asid(Mem_unit::Asid_invalid);
}

PROTECTED inline NEEDS[<new>, "kmem_slab.h", "kmem_space.h", Mem_space::asid]
bool
Mem_space::initialize()
{
  _dir = _dir_alloc.q_new(ram_quota());
  if (!_dir)
    return false;

  _dir->clear(Pte_ptr::need_cache_write_back(false));
  _dir_phys = Phys_mem_addr(Kmem_space::kdir()->virt_to_phys((Address)_dir));

  return true;
}

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type* pdir)
  : _quota(q), _dir (pdir)
{
  asid(Mem_unit::Asid_invalid);
  _current.cpu(Cpu_number::boot_cpu()) = this;
  _dir_phys = Phys_mem_addr(Kmem_space::kdir()->virt_to_phys((Address)_dir));
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
IMPLEMENTATION [armv5 || armv6 || armv7 || armv8]:

IMPLEMENT inline
void
Mem_space::v_set_access_flags(Vaddr, L4_fpage::Rights)
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [armv5]:

PRIVATE inline
void
Mem_space::asid(unsigned long)
{}

PRIVATE inline
void
Mem_space::reset_asid()
{}

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
INTERFACE [armv6 || arm_lpae]:

EXTENSION class Mem_space
{
  enum
  {
    Asid_num = 256,
    Asid_base = 0
  };
};

//----------------------------------------------------------------------------
INTERFACE [(armv7 || armv8) && !arm_lpae]:

EXTENSION class Mem_space
{
  enum
  {
    Asid_num = 255,
    Asid_base = 1
  };
};

//----------------------------------------------------------------------------
INTERFACE [armv6 || armv7 || armv8]:

#include "id_alloc.h"
#include "types.h"

EXTENSION class Mem_space
{
public:
  enum { Have_asids = 1 };
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

  struct Asid_alloc : Id_alloc<unsigned char, Mem_space, Asid_ops>
  {
    Asid_alloc() : Id_alloc<unsigned char, Mem_space, Asid_ops>(Asid_num) {}
  };

  static Per_cpu<Asid_alloc> _asid_alloc;
};

//----------------------------------------------------------------------------
INTERFACE [!(armv6 || armv7 || armv8)]:

EXTENSION class Mem_space
{
public:
  enum { Have_asids = 0 };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [armv6 || armv7 || armv8]:

DEFINE_PER_CPU Per_cpu<Mem_space::Asid_alloc> Mem_space::_asid_alloc;

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

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));
  add_page_size(Page_order(21)); // 2MB
  add_page_size(Page_order(30)); // 1GB
}

