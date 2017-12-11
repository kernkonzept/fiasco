/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 * (2015)
 * Author: Alexander Warg <alexander.warg@kernkonzept.com>
 */

INTERFACE [mips]:

#include "auto_quota.h"
#include "kmem.h"               // for "_unused_*" virtual memory regions
#include "member_offs.h"
#include "mem_unit.h"
#include "paging.h"
#include "types.h"
#include "ram_quota.h"
#include "id_alloc.h"

EXTENSION class Mem_space
{
  friend class Jdb;

public:
  typedef Pdir Dir_type;
  enum { Have_asids = 1 };

  /** Return status of v_insert. */
  enum // Status
  {
    Insert_ok = 0,              ///< Mapping was added successfully.
    Insert_warn_exists,         ///< Mapping already existed
    Insert_warn_attrib_upgrade, ///< Mapping already existed, attribs upgrade
    Insert_err_nomem,           ///< Couldn't alloc new page table
    Insert_err_exists           ///< A mapping already exists at the target addr
  };

  // Mapping utilities

  enum                          // Definitions for map_util
  {
    Need_insert_tlb_flush = 0,
    Map_page_size = Config::PAGE_SIZE,
    Page_shift = Config::PAGE_SHIFT,
    Whole_space = MWORD_BITS,
    Identity_map = 0,
  };


  static void kernel_space(Mem_space *);

private:
  // DATA
  Dir_type *_dir;

  typedef Per_cpu_array<unsigned long> Asid_array;
  Asid_array _asid;

  struct Asid_ops
  {
    enum { Id_offset = 0 };

    static bool valid(Mem_space *o, Cpu_number cpu)
    { return o->_asid[cpu] != ~0UL; }

    static unsigned long get_id(Mem_space *o, Cpu_number cpu)
    { return o->_asid[cpu]; }

    static bool can_replace(Mem_space *v, Cpu_number cpu)
    { return v != current_mem_space(cpu); }

    static void set_id(Mem_space *o, Cpu_number cpu, unsigned long id)
    {
      write_now(&o->_asid[cpu], id);
      Mem_unit::tlb_flush(id, 0);
    }

    static void reset_id(Mem_space *o, Cpu_number cpu)
    { write_now(&o->_asid[cpu], ~0UL); }
  };

  struct Asid_alloc : Id_alloc<unsigned char, Mem_space, Asid_ops>
  {
    Asid_alloc() : Id_alloc<unsigned char, Mem_space, Asid_ops>(256) {}
  };

  static Per_cpu<Asid_alloc> _asid_alloc;
};


//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

#include <cassert>
#include <cstdio>
#include <cstring>
#include <new>

#include "asm_mips.h"
#include "atomic.h"
#include "config.h"
#include "globals.h"
#include "kdb_ke.h"
#include "l4_types.h"
#include "panic.h"
#include "paging.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "cpu.h"

DEFINE_PER_CPU Per_cpu<Mem_space::Asid_alloc> Mem_space::_asid_alloc;


/** Constructor.  Creates a new address space and registers it with
  * Space_index.
  *
  * Registration may fail (if a task with the given number already
  * exists, or if another thread creates an address space for the same
  * task number concurrently).  In this case, the newly-created
  * address space should be deleted again.
  */
PUBLIC inline NEEDS[Mem_space::guest_id_init]
Mem_space::Mem_space(Ram_quota *q)
: _quota(q), _dir(0)
{
  asid(~0UL);
  guest_id_init();
}

PROTECTED inline NEEDS[<new>, "kmem_alloc.h", Mem_space::asid]
bool
Mem_space::initialize()
{
  Auto_quota<Ram_quota> q(ram_quota(), sizeof(Dir_type));
  if (EXPECT_FALSE(!q))
    return false;

  _dir = (Dir_type*)Kmem_alloc::allocator()->unaligned_alloc(sizeof(Dir_type));
  if (!_dir)
    return false;

  _dir->clear();

  q.release();
  return true;
}

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type* pdir)
  : _quota(q), _dir(pdir)
{
  asid(~0UL);
  guest_id_init();
  _current.cpu(Cpu_number::boot_cpu()) = this;
}

PUBLIC static inline
bool
Mem_space::is_full_flush(L4_fpage::Rights rights)
{
  return (bool)(rights & L4_fpage::Rights::R());
}

// Mapping utilities

IMPLEMENT inline
Mem_space *
Mem_space::current_mem_space(Cpu_number cpu)
{
  return _current.cpu(cpu);
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
  assert (cxx::is_zero(cxx::get_lsb(phys, size)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  unsigned po = cxx::int_value<Page_order>(size);
  auto i = _dir->walk(virt, po, Kmem_alloc::q_allocator(_quota));

  if (EXPECT_FALSE(i.size != po && !i.is_pte()))
    return Insert_err_nomem;

  if (EXPECT_FALSE(i.size != po)) // exists with different size
    return Insert_err_exists;

  if (EXPECT_FALSE(i.is_pte() && (i.page_addr() != phys)))
    return Insert_err_exists;

  apply_extra_page_attribs(&page_attribs);

  if (EXPECT_FALSE(i.is_pte()))
    {
      // upgrade
      page_attribs.rights |= i.rights();
      Mword x = i.make_page(phys, page_attribs);
      if (x == *i.e)
        return Insert_warn_exists;

      *i.e = x;
      // FIXME: sync TLB
      return Insert_warn_attrib_upgrade;
    }
  else
    {
      *i.e = i.make_page(phys, page_attribs);
      // FIXME: sync TLB
      return Insert_ok;
    }
}

IMPLEMENT
void
Mem_space::v_set_access_flags(Vaddr, L4_fpage::Rights)
{
  // not supported currently
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
  if (EXPECT_TRUE(dir() != 0))
    return dir()->virt_to_phys(virt);
  return ~0UL;
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
 * This method is similar to Mem_space's lookup().
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
  if (EXPECT_TRUE(dir() != 0))
    return dir()->virt_to_phys((Address)a);

  return ~0UL;
}

IMPLEMENT
bool
Mem_space::v_lookup(Vaddr const virt, Phys_addr *phys,
                    Page_order *order, Attr *page_attribs) override
{
  auto i = _dir->walk(virt);
  if (order) *order = Page_order(i.size);

  if (!i.is_pte())
    return false;

  if (phys) *phys = i.page_addr();
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

IMPLEMENT
L4_fpage::Rights
Mem_space::v_delete(Vaddr virt, Page_order size,
                    L4_fpage::Rights page_attribs)
{
  (void)size;
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));
  auto i = _dir->walk(virt);

  if (EXPECT_FALSE (! i.is_pte()))
    return L4_fpage::Rights(0);

  L4_fpage::Rights ret = i.rights();

  if (! (page_attribs & L4_fpage::Rights::R()))
    {
      Mword x = i.del_rights(*i.e, page_attribs);
      if (x == *i.e)
        return ret;

      *i.e = x;
    }
  else
    i.clear();

  //i.update_tlb(Virt_addr::val(virt), c_asid(), c_vzguestid());

  return ret;
}

PUBLIC inline NEEDS[Mem_space::set_guest_ctl1_rid, <cstdio>]
bool
Mem_space::add_tlb_entry(Vaddr virt, bool write_access, bool need_probe, bool guest)
{ (void) write_access;
  // align virt to double pages at least, as we need to add
  // two phys pages in the tlb
  Vaddr a = cxx::mask_lsb(virt, Page_order(Config::PAGE_SHIFT + 1));

  auto e = _dir->walk(virt);
  if (EXPECT_FALSE(!e.is_pte()))
    return false;

  if (EXPECT_FALSE(write_access && !e.is_writable()))
    return false;

  Mword e0, e1, pm;
  if (EXPECT_FALSE(e.size != Config::PAGE_SHIFT))
    {
      // super page, odd page sizes only supported
      assert (e.size & 1);
      e0 = e.e[0];
      e0 >>= Pdir::PWField_ptei - 2;
      e0 = (e0 & 3) << (MWORD_BITS - 2) | (e0 >> 2);
      e1 = e0 | (1UL << (e.size - 1 - Pdir::PWField_ptei));
      pm = ((1UL << e.size) - 1) & ~0x1fff;
    }
  else
    {
      // dual page mode (at leaf level)
      bool odd_page = !cxx::is_zero(virt & Vaddr(Virt_addr(1) << Page_order(Config::PAGE_SHIFT)));
      e.e -= odd_page;
      e0 = e.e[0];
      e0 >>= Pdir::PWField_ptei - 2;
      e0 = (e0 & 3) << (MWORD_BITS - 2) | (e0 >> 2);
      e1 = e.e[1];
      e1 >>= Pdir::PWField_ptei - 2;
      e1 = (e1 & 3) << (MWORD_BITS - 2) | (e1 >> 2);
      pm = ((2UL << Config::PAGE_SHIFT) - 1) & ~0x1fffUL;
      static_assert ((Config::PAGE_SHIFT & 1) == 0, "odd page sizes not supported");
    }

  Mem_unit::page_mask(pm);
  if (0)
    printf("TLB: sz=%u v=%lx p=%lx e0=%lx e1=%lx\n", e.size,
           cxx::int_value<Virt_addr>(virt), e.e[0], e0, e1);

  // assert (c_asid() == Mem_unit::entry_hi() & 0xff);
  Mword eh = cxx::int_value<Virt_addr>(a);

  if (!guest)
    eh |= c_asid();

  Mem_unit::entry_hi(eh);
  set_guest_ctl1_rid(guest);

  bool use_wr = true;
  if (need_probe)
    {
      Mips::ehb();
      Unsigned32 idx = Mem_unit::tlb_probe();
      use_wr = idx & (1UL << 31);
    }
  Mem_unit::entry_lo0(e0);
  Mem_unit::entry_lo1(e1);
  Mips::ehb();
  if (use_wr)
    asm volatile ("tlbwr");
  else
    asm volatile ("tlbwi");

  return true;
}

/**
 * \brief Free all memory allocated for this Mem_space.
 * \pre Runs after the destructor!
 */
PUBLIC
Mem_space::~Mem_space()
{
  reset_asid();
  reset_guest_id();
  if (_dir)
    {
      // free all page tables we have allocated for this address space
      // except the ones in kernel space which are always shared
      _dir->destroy(Virt_addr(0UL),
                    Virt_addr(Mem_layout::User_max),
                    Kmem_alloc::q_allocator(_quota));
      Kmem_alloc::allocator()->q_unaligned_free(ram_quota(), sizeof(Dir_type), _dir);
    }
}

PROTECTED inline int Mem_space::sync_kernel() { return 0; }


PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{ return v; }

PUBLIC static
void
Mem_space::init_page_sizes()
{
  add_page_size(Page_order(Config::PAGE_SHIFT));

  if (Config::have_superpages)
    add_page_size(Page_order(Config::SUPERPAGE_SHIFT));
}

PUBLIC static
void
Mem_space::init()
{
  Tlb_entry::cached = Mips::Cfg<0>::read().k0() << 3;
  printf("TLB CCA: %ld\n", Tlb_entry::cached >> 3);

  init_page_sizes();
  //init_vzguestid();
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

IMPLEMENT inline
void
Mem_space::make_current()
{
  // asign asid if not yet done!
  Mem_unit::set_current_asid(asid());
  _current.current() = this;
}

//--------------------------------------------------------------
IMPLEMENTATION [!mips_vz]:

IMPLEMENT inline NEEDS ["kmem.h", Mem_space::c_asid]
void Mem_space::switchin_context(Mem_space *)
{
#if 0
  // never switch to kernel space (context of the idle thread)
  if (this == kernel_space())
    return;
#endif

  make_current();
}

IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool force = false)
{
  if (force)
    Mem_unit::tlb_flush(c_asid(), 0);
}


PRIVATE inline void Mem_space::guest_id_init() {}
PRIVATE inline void Mem_space::reset_guest_id() {}
PRIVATE inline void Mem_space::set_guest_ctl1_rid(bool) {}
PRIVATE inline void Mem_space::apply_extra_page_attribs(Attr *) {}

//-----------------------------------------------------------------
INTERFACE [mips_vz]:

#include "cpu.h"
#include "mem_unit.h"
#include "vz.h"

EXTENSION class Mem_space
{
protected:
  bool _is_vz_guest = false;

private:
  typedef Per_cpu_array<unsigned char> Guest_id_array;
  Guest_id_array _guest_id;

  struct Guest_id_ops
  {
    enum { Id_offset = 1 };

    static bool valid(Mem_space *o, Cpu_number cpu)
    { return o->_guest_id[cpu] != 0; }

    static unsigned get_id(Mem_space *o, Cpu_number cpu)
    { return o->_guest_id[cpu]; }

    static bool can_replace(Mem_space *v, Cpu_number cpu)
    { return v->_guest_id[cpu] != Vz::owner.cpu(cpu).guest_id; }

    static void set_id(Mem_space *o, Cpu_number cpu, int id)
    {
      write_now(&o->_guest_id[cpu], (unsigned char)id);
      Mem_unit::tlb_flush(-1, id);
      Mem_unit::vz_guest_tlb_flush(id);
    }

    static void reset_id(Mem_space *o, Cpu_number cpu)
    { write_now(&o->_guest_id[cpu], (unsigned char)0); }
  };

  struct Guest_id_alloc : Id_alloc<unsigned char, Mem_space, Guest_id_ops>
  {
    static unsigned n_guest_ids(Cpu_number cpu)
    {
      if (!Cpu::cpus.cpu(cpu).options.vz())
        return 0;

      Mword x;
      asm volatile (
          ".set push \n"
          ".set noat \n"
          "mfc0 $1, $10, 4 \n"
          "move %0, $1 \n"
          "ori %0, 0xff \n"
          "mtc0 %0, $10, 4 \n"
          "ehb \n"
          "mfc0 %0, $10, 4 \n"
          "mtc0 $1, $10, 4 \n"
          "ehb \n" // overly paranoid ehb here, usually the guest ID is first
                   // used when accessing mapped memory, or by the GIC
          ".set pop"
          : "=r"(x));

      return (x & 0xff);
    }

    Guest_id_alloc(Cpu_number cpu)
    : Id_alloc<unsigned char, Mem_space, Guest_id_ops>(n_guest_ids(cpu))
    {}
  };

  static Per_cpu<Guest_id_alloc> _guest_id_alloc;
};

//-----------------------------------------------------------------
IMPLEMENTATION [mips_vz]:

#include "alternatives.h"

DEFINE_PER_CPU Per_cpu<Mem_space::Guest_id_alloc>
  Mem_space::_guest_id_alloc(Per_cpu_data::Cpu_num);

PRIVATE inline
void
Mem_space::apply_extra_page_attribs(Attr *a)
{
  // ATTENTION: Setting the global bit for VM page-tables prevents us
  // from running normal threads in this mem-space, hence
  // caps & Caps::threads() must be false.
  if (_is_vz_guest)
    a->kern |= Page::Kern::Global();
}

#include "logdefs.h"
PRIVATE inline
void
Mem_space::guest_id_init()
{
  for (Guest_id_array::iterator i = _guest_id.begin(); i != _guest_id.end(); ++i)
    *i = 0;
}

IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool force = false)
{
  if (!force)
    return;

  Cpu_number cpu = current_cpu();
  Mem_unit::tlb_flush(Asid_ops::get_id(this, cpu),
                      Guest_id_ops::get_id(this, cpu));
}

PRIVATE inline
void
Mem_space::reset_guest_id()
{
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    _guest_id_alloc.cpu(i).free(this, i);
}

PRIVATE inline NEEDS ["alternatives.h"]
void
Mem_space::set_guest_ctl1_rid(bool guest)
{
  unsigned long gid = 0;
  if (guest)
      gid = _guest_id[current_cpu()];
  asm volatile (ALTERNATIVE_INSN(
        "nop",
        ".set push; .set noat; mfc0 $1, $10, 4; ins $1, %0, 16, 8; mtc0 $1, $10, 4; .set pop",
        0x4 /* FEATURE_VZ */)
      : : "r"(gid));
}

IMPLEMENT inline NEEDS ["kmem.h", Mem_space::asid, "logdefs.h", "alternatives.h"]
void Mem_space::switchin_context(Mem_space *)
{
#if 0
  // never switch to kernel space (context of the idle thread)
  if (this == kernel_space())
    return;
#endif
  asm volatile (ALTERNATIVE_INSN(
        "nop",
        "mtc0 %0, $10, 4",  // Load GuestCtl1 with guest ID
        0x4 /* FEATURE_VZ */)
      : : "r"(0));

  Mem_unit::set_current_asid(asid());
  _current.current() = this;
  // no ehb here as we use the mappings after the eret only
}

PUBLIC inline NEEDS ["kmem.h", Mem_space::asid, "logdefs.h", "alternatives.h"]
unsigned Mem_space::switchin_guest_context()
{
  auto c = current_cpu();
  unsigned guest_id = _guest_id_alloc.cpu(c).alloc(this, c);

  asm volatile (ALTERNATIVE_INSN(
        "nop",
        "mtc0 %0, $10, 4",  // Load GuestCtl1 with guest ID
        0x4 /* FEATURE_VZ */)
      : : "r"(guest_id));

  Mem_unit::set_current_asid(0);
  _current.current() = this;
  // no ehb here as we use the mappings after the eret only
  return guest_id;
}

