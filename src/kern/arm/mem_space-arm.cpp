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



/**
 * \brief Free all memory allocated for this Mem_space.
 * \pre Runs after the destructor!
 */
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


/** Constructor.  Creates a new address space and registers it with
  * Space_index.
  *
  * Registration may fail (if a task with the given number already
  * exists, or if another thread creates an address space for the same
  * task number concurrently).  In this case, the newly-created
  * address space should be deleted again.
  */
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

/*
   The ARM reference manual suggests to use the same address space id
   across multiple CPUs. Here we introduce a global allocation scheme
   for ASIDs that uses a tuple of <generation, asid> to quickly decide
   whether an address space id is valid or not.

   Address space ids become invalid if all ASIDs are allocated and an
   address space needs a new ASID. In this case we invalidate all
   ASIDs except the ones currently used on a CPU by increasing the
   generation and starting to allocate new ASIDs.

   The scheme relies on three fields, "active", "reserved" and
   "generation":
   * "generation" is a global variable keeping track of the current
      generation of ASIDs.
   * "active" keeps track of the address space id which is currently used
      on a CPU.
   * "reserved" keeps track of asids active during a generation change

   If the asid of an address space has an old generation or "active"
   contains an invalid asid we might need a new ASID. So we enter a
   critical region and

   * check whether the ASID is a reserved one which can stay as is,
     otherwise allocate a new ASID if possible
   * if no ASID is available
     * invalidate allocated ASIDs by incrementing the generation and by
       resetting the reserved bitmap
     * save all valid active ASIDs in "reserved" and mark them as reserved
       in a bitmap
     * set active ASID to invalid
     * mark a tlb flush pending

   "active", "generation" and the asid attribut of a mem_space are
   read outside of the critical region and are therefore manipulated
   using atomic operations.

   The usage of a counter for the generation may lead to two address
   spaces having the same <generation, asid> tuple after the
   generation overflows and starts with 0 again. With 32 bit and
   permanent ASID allocation this takes about 400 seconds and the
   address space has a probability of 2^-24 to get the same generation
   number. With 64bit it takes about 50000 years.

*/

EXTENSION class Mem_space
{
private:
  /** Asid storage format:
   * 63                        X      0
   * +------------------------+--------+
   * |   generation count     | ASID   |
   * +------------------------+--------+
   * X = Asid_bits - 1
   *
   * As the generation count increases it might happen that it wraps
   * around and starts at 0 again. If we have address spaces which are
   * not active for a "long time" we might see <generation, asid>
   * tuples of the same value for different spaces after a wrap
   * around. To decrease the likelyhood that this actually happens we
   * use a large generation count. Under worst case assumptions
   * (working set constantly generating new ASIDs every 100 cycles on
   * a 1GHz processor) a wrap around happens after 429 seconds with 32
   * bits and after 58494 years with 64 bit. We use 64bit to be on the
   * save side.
   */
  struct Asid
  {
    enum
    {
      Generation_inc = 1ULL << Asid_bits,
      Mask      = Generation_inc - 1,
      Invalid   = ~0ULL,
    };

    typedef unsigned long long Value;

    Value a;

    Asid() = default;
    Asid(unsigned long long a) : a(a) {}

    bool is_valid() const
    {
      if (sizeof(a) == sizeof(Mword))
        return a != Invalid;
      else
        return ((Unsigned32)(a >> 32) & (Unsigned32)a) != Unsigned32(~0);
    }

    bool is_invalid_generation() const
    {
      return a == (Invalid & ~Mask);
    }

    Value asid() const { return a & Mask; }

    bool is_same_generation(Asid generation) const
    { return (a & ~Mask) == generation.a; }

    bool operator == (Asid o) const
    { return a == o.a; }

    bool operator != (Asid o) const
    { return a != o.a; }
  };

  enum
  {
    // Allocate ASIDs in [Asid_base, Debug_asid_allocation] if
    // Debug_asid_allocation != 0
    Debug_asid_allocation = 0,
  };

public:
  /**
   * Keep track of reserved Asids
   *
   * If a generation roll over happens we have to keep track of ASIDs
   * active on other CPUs. These ASIDs are marked as reserved in the
   * bitmap.
   */
  class Asid_bitmap : public Bitmap<Asid_num>
  {
  public:
    /**
     * Reset all bits and set first available ASID to Asid_base
     */
    void reset()
    {
      clear_all();
      _current_idx = Asid_base;
    }

    /**
     * Find next free ASID
     *
     * \return First free ASID or Asid_num if no ASID is available
     */
    unsigned find_next();

    Asid_bitmap()
    {
      reset();
    }

  private:
    unsigned _current_idx;
  };

  /**
   * Per CPU active and reserved ASIDs
   */
  struct Asids
  {
    /**
     * Currently active ASID on a CPU.
     *
     * written using atomic_xchg outside of spinlock and
     * atomic_write under protection of spinlock
     */
    Asid active = Asid::Invalid;

    /**
     * reserved ASID on a CPU, active during last generation change.
     *
     * written under protection of spinlock
     */
    Asid reserved = Asid::Invalid;
  };

public:
  enum { Have_asids = 1 };

private:
  /// active/reserved ASID (per CPU)
  static Per_cpu<Asids> _asids;

  /// Protect elements changed during generation roll over
  static Spin_lock<> _asid_lock;

  /// current ASID generation, protected by _asid_lock
  static Asid _generation;

  /// keep track of reserved ASIDs, protected by _asid_lock
  static Asid_bitmap _asid_bitmap;

  /// keep track of pending TLB flush operations, protected by _asid_lock
  static Cpu_mask _tlb_flush_pending;

  /// current asid of mem_space, protected by _asid_lock
  Asid _asid = Asid::Invalid;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v6 || arm_v7 || arm_v8]:
#include "cpu.h"
#include "cpu_lock.h"
#include "lock_guard.h"

IMPLEMENT unsigned
Mem_space::Asid_bitmap::find_next()
{
  if (Debug_asid_allocation && _current_idx > Debug_asid_allocation)
    return Asid_num;

  // assume a sparsely populated bitmap - the next free bit is
  // normally found during first iteration
  for (unsigned i = _current_idx; i < Asid_num; ++i)
    if (operator[](i) == 0)
      {
        _current_idx = i + 1;
        return i;
      }

  return Asid_num;
}

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

PRIVATE static inline NEEDS["cpu.h"]
bool
Mem_space::check_and_update_reserved(Asid asid, Asid update)
{
  bool res = false;

  for (Cpu_number cpu = Cpu_number::first(); cpu < Config::max_num_cpus();
       ++cpu)
    {
      if (!Cpu::online(cpu))
        continue;

      if (_asids.cpu(cpu).reserved == asid)
        {
          _asids.cpu(cpu).reserved = update;
          res = true;
        }
    }
  return res;
}

/**
 * Reset allocation data structures, reserve currently active ASIDs,
 * mark TLB flush pending for all CPUs
 *
 * \pre
 *   * _asid_lock held
 *
 * \post
 *   * _asids.cpu(x).reserved == ASID currently used on cpu x
 *   * _asids.cpu(x).active   == Mem_unit::Asid_invalid
 *   * _asid_bitmap[x]   != 0 for x in { _asdis.cpu(cpu).reserved }
 *   * _asid_lock held
 *
 */
PRIVATE static inline NEEDS["atomic.h"]
void
Mem_space::roll_over()
{
  _asid_bitmap.reset();

  // update reserved asids
  for (Cpu_number cpu = Cpu_number::first(); cpu < Config::max_num_cpus();
       ++cpu)
    {
      if (!Cpu::online(cpu))
        continue;

      Asid asid = atomic_exchange(&_asids.cpu(cpu).active, Asid::Invalid);

      // keep reserved asid, if there already was a roll over
      if (asid.is_valid())
        _asids.cpu(cpu).reserved = asid;
      else
        asid = _asids.cpu(cpu).reserved;

      if (asid.is_valid())
        _asid_bitmap.set_bit(asid.asid());
    }

  _tlb_flush_pending = Cpu::online_mask();
}

/**
 * Get a new ASID
 *
 * Check whether the ASID is a reserved one (was in use on any cpu
 * during roll over). If it is, update generation and return.
 * Otherwise allocate a new one and handle generation roll over if
 * necessary.
 *
 * \pre
 *   * _asid_lock held
 *
 * \post
 *   * if a generation roll over happens, generation increased by Generation_inc
 *   * returned ASID is marked in _asid_bitmap and has current generation
 *   * _asid_lock held
 *
 */
PRIVATE
Mem_space::Asid FIASCO_FLATTEN
Mem_space::new_asid(Asid asid, Asid generation)
{
  if (asid.is_valid() && _asid_bitmap[asid.asid()])
    {
      Asid update = asid.asid() | generation.a;
      if (EXPECT_TRUE(check_and_update_reserved(asid, update)))
        {
          // This ASID was active during a roll over and therefore is
          // still valid. Return the asid with its updated generation.
          return update;
        }
    }

  // Get a new ASID
  unsigned new_asid = _asid_bitmap.find_next();
  if (EXPECT_FALSE(new_asid == Asid_num))
    {
      generation = atomic_add_fetch(&_generation, Asid::Generation_inc);

      if (EXPECT_FALSE(generation.is_invalid_generation()))
        {
          // Skip problematic generation value
          generation = atomic_add_fetch(&_generation, Asid::Generation_inc);
        }

      roll_over();
      new_asid = _asid_bitmap.find_next();
    }

  return new_asid | generation.a;
}

/**
 * Get a valid ASID
 *
 * Check validity of current ASID. If it is valid, return the
 * current ASID, otherwise allocate a new one and invalidate TLB if
 * necessary.
 */
PUBLIC inline NEEDS["atomic.h"]
unsigned long
Mem_space::asid()
{

  auto *active_asid = &_asids.current().active;

    {
      Asid asid       = atomic_load(&_asid);
      Asid generation = atomic_load(&_generation);

      // is_same_generation implicitely checks for asid != Asid_invalid
      if (   EXPECT_TRUE(asid.is_same_generation(generation))
          && EXPECT_TRUE(atomic_exchange(active_asid, asid).is_valid()))
        return asid.asid();
    }

  auto guard = lock_guard(_asid_lock);

  // Re-read data
  Asid asid       = atomic_load(&_asid);
  Asid generation = atomic_load(&_generation);

  // We either have an older generation or a roll over happened on
  // another cpu - find out which one it was
  if (!asid.is_same_generation(generation))
    {
      // We have an asid from an older generation - get a fresh one
      asid = new_asid(asid, generation);
      atomic_store(&_asid, asid);
    }

  // Is a tlb flush pending?
  if (_tlb_flush_pending.atomic_get_and_clear(current_cpu()))
    {
      Mem_unit::tlb_flush();
      Mem::dsb();
    }

  // Set active asid, needs to be atomic since this value is written
  // above using atomic_xchg()
  atomic_store(active_asid, asid);

  return asid.asid();
};

DEFINE_PER_CPU Per_cpu<Mem_space::Asids> Mem_space::_asids;
Mem_space::Asid        Mem_space::_generation = Mem_space::Asid::Generation_inc;
Mem_space::Asid_bitmap Mem_space::_asid_bitmap;
Cpu_mask               Mem_space::_tlb_flush_pending;
Spin_lock<>            Mem_space::_asid_lock;

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
