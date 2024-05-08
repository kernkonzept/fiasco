INTERFACE[riscv]:

#include <cassert>

#include "kmem.h"
#include "kmem_slab.h"
#include "paging.h"

EXTENSION class Mem_space
{
  friend class Mem_space_test;

public:
  typedef Pdir Dir_type;

    /** Return status of v_insert. */
  enum // Status
  {
    Insert_ok = 0,    ///< Mapping was added successfully.
    Insert_warn_exists,   ///< Mapping already existed
    Insert_warn_attrib_upgrade, ///< Mapping already existed, attribs upgrade
    Insert_err_nomem,   ///< Couldn't alloc new page table
    Insert_err_exists   ///< A mapping already exists at the target addr
  };

  // Mapping utilities
  enum        // Definitions for map_util
  {
    Need_insert_tlb_flush = 1,
    Map_page_size = Config::PAGE_SIZE,
    Page_shift = Config::PAGE_SHIFT,
    Whole_space = MWORD_BITS,
    Identity_map = 0,
  };

  static void kernel_space(Mem_space *);

private:
  Dir_type *_dir;
  Phys_mem_addr _dir_phys;

  static Kmem_slab_t<Dir_type, sizeof(Dir_type)> _dir_alloc;
};

//----------------------------------------------------------------------------
INTERFACE [riscv && riscv_asid]:

#include "asid_alloc.h"
#include "cpu.h"
#include "mem_unit.h"

EXTENSION class Mem_space
{
private:
  enum
  {
    Asid_base = 0,
    Asid_bits = Cpu::Satp_asid_bits,
  };

  /// As RV32 has no support for 64-bit atomic operations, we have to use a
  /// 32-bit ASID counter there.
  using Asid_value_type = Mword;

  using Asid_alloc = Asid_alloc_t<Asid_value_type, Asid_bits, Asid_base,
                                  Mem_unit::asid_num>;
  using Asid = Asid_alloc::Asid;
  using Asids = Asid_alloc::Asids_per_cpu;

  /// active/reserved ASID (per CPU)
  static Per_cpu<Asids> _asids;
  static Asid_alloc _asid_alloc;

  /// current ASID of mem_space, provided by _asid_alloc
  Asid _asid = Asid::Invalid;
};

//----------------------------------------------------------------------------
IMPLEMENTATION[riscv]:

#include "cpu.h"
#include "kmem_alloc.h"
#include "logdefs.h"

PUBLIC explicit inline
Mem_space::Mem_space(Ram_quota *q) : _quota(q), _dir(nullptr) {}

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type *pdir)
  : _quota(q), _dir(pdir)
{
  _current.cpu(Cpu_number::boot_cpu()) = this;

  Address dir_addr = reinterpret_cast<Address>(_dir);
  _dir_phys = Phys_mem_addr(Kmem::kdir->virt_to_phys(dir_addr));
}

PROTECTED inline
bool
Mem_space::initialize()
{
  _dir = _dir_alloc.q_new(ram_quota());
  if (!_dir)
    return false;

  _dir->clear(true);
  _dir_phys =
    Phys_mem_addr(Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(_dir)));

  return true;
}

PUBLIC static inline
bool
Mem_space::is_full_flush(L4_fpage::Rights rights)
{
  return static_cast<bool>(rights & L4_fpage::Rights::R());
}

PUBLIC static
void
Mem_space::init_page_sizes()
{
  // Any level of PTE may be a leaf PTE.
  for (int level = Pdir::Depth; level >= 0; level--)
    add_page_size(Page_order(Pdir::page_order_for_level(level)));
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

PROTECTED inline
int
Mem_space::sync_kernel()
{
  Virt_addr kernel_base(
    canonize(Page_number(Virt_addr(Mem_layout::User_max + 1))));

  return _dir->sync(kernel_base, kernel_space()->_dir, kernel_base,
                    Virt_size(-cxx::int_value<Virt_addr>(kernel_base)),
                    Pdir::Super_level, false, Kmem_alloc::q_allocator(_quota));
}

IMPLEMENT inline
Mem_space::Tlb_type
Mem_space::regular_tlb_type()
{
  return Mem_unit::Have_asids ? Tlb_per_cpu_asid : Tlb_per_cpu_global;
}

IMPLEMENT inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush_current_cpu()
{
  return tlb_flush_space(true);
}

PUBLIC inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush_space(bool with_asid)
{
  if (!Mem_unit::Have_asids || (with_asid && c_asid() != Mem_unit::Asid_invalid))
    {
      // Even with ASIDs disabled its still preferable to pass an ASID
      // to tlb_flush(), in this case Mem_unit::Asid_disabled,
      // to avoid flushing of global pages.
      Mem_unit::tlb_flush(c_asid());
      tlb_mark_unused_if_non_current();
    }
}

IMPLEMENT inline
void
Mem_space::kernel_space(Mem_space *kernel_space)
{
  _kernel_space = kernel_space;
}

IMPLEMENT
Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                    Attr page_attribs, bool)
{
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
      return Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      return Insert_ok;
    }
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
Mem_space::v_delete(Vaddr virt, [[maybe_unused]] Page_order size,
                    L4_fpage::Rights page_attribs)
{
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));
  auto i = _dir->walk(virt);

  if (EXPECT_FALSE (! i.is_valid()))
    return L4_fpage::Rights(0);

  L4_fpage::Rights ret = i.access_flags();

  if (! (page_attribs & L4_fpage::Rights::R()))
    i.del_rights(page_attribs);
  else
    i.clear();

  return ret;
}

IMPLEMENT inline
void
Mem_space::v_set_access_flags(Vaddr, L4_fpage::Rights)
{}

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
      _dir->destroy(Virt_addr(Mem_layout::User_max + 1),
                    Virt_addr(Pdir::Max_addr), 0, Pdir::Super_level,
                    Kmem_alloc::q_allocator(_quota));
      _dir_alloc.q_free(ram_quota(), _dir);
    }
}

IMPLEMENT inline NEEDS["cpu.h", "mem_layout.h", Mem_space::asid_with_fence]
void
Mem_space::make_current(Switchin_flags)
{
  assert(_dir_phys);
  // On RISC-V the TLB maintenance instruction is specified not as a TLB flush
  // but as a fence that orders the implicit memory accesses to the page tables
  // made by subsequent instruction execution in respect to preceding explicit
  // memory accesses. Thus, in case this memory space's ASID has changed since
  // the last modification of its page table (including a TLB fence with the old
  // ASID), we have to execute a TLB fence with the new ASID before using the
  // page table.
  Cpu::set_satp(asid_with_fence(),
                Cpu::phys_to_ppn(cxx::int_value<Phys_mem_addr>(_dir_phys)));
  // If ASIDs are not supported, flush the TLB after switching the page table.
  tlb_flush_space(false);
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

PRIVATE inline
unsigned long
Mem_space::asid_with_fence()
{
  // Detecting an ASID change since the last page table modification would be
  // difficult in a multi-processor scenario, most likely requiring adding a
  // per CPU flag of some kind, which is checked here.
  // Fortunately there is already the Mem_space::tlb_active_on_cpu per CPU flag,
  // which is used to coordinate cross-CPU TLB maintenance. The flag is set for
  // a memory space for the current CPU whenever it becomes active on the CPU.
  // The flag is cleared when a TLB fence is executed for the memory space,
  // while it is not the current memory space on the CPU. After setting the
  // flag, the Mem_space::sync_write_tlb_active_on_cpu() method is invoked,
  // which takes care of executing a TLB fence. This ensures, that even if the
  // memory space previously had an invalid ASID or an ASID rollover took place
  // whereby it got a new ASID, that an appropriate TLB fence, i.e. with an
  // up-to-date ASID, is executed before the memory space is made current.
  return asid();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !mp]:

PRIVATE inline
unsigned long
Mem_space::asid_with_fence()
{
  // When a new ASID is allocated, either because the memory space did not have
  // one or an ASID rollover took place, we need to execute a TLB fence with the
  // new ASID.
  unsigned long current_asid = c_asid();
  unsigned long new_asid = asid();
  if (current_asid != new_asid)
    Mem_unit::tlb_flush(new_asid);
  return new_asid;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && 32bit]:

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{
  return v;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && riscv_sv39]:

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{
  if (v & Page_number(Virt_addr(1UL << 38)))
    v = v | Page_number(Virt_addr(~0UL << 38));
  return v;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && riscv_sv48]:

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{
  if (v & Page_number(Virt_addr(1UL << 47)))
    v = v | Page_number(Virt_addr(~0UL << 47));
  return v;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && riscv_asid]:

#include "atomic.h"

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

PUBLIC inline NEEDS["asid_alloc.h"]
unsigned long
Mem_space::asid()
{
  if (_asid_alloc.get_or_alloc_asid(&_asid))
    Mem_unit::tlb_flush();

  return _asid.asid();
};

DEFINE_PER_CPU Per_cpu<Mem_space::Asids> Mem_space::_asids;
Mem_space::Asid_alloc  Mem_space::_asid_alloc(&_asids);

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !riscv_asid]:

PUBLIC inline
Mword FIASCO_PURE
Mem_space::c_asid() const
{ return Mem_unit::Asid_disabled; }

PUBLIC inline
Mword
Mem_space::asid()
{ return Mem_unit::Asid_disabled; }

//-----------------------------------------------------------------------------
IMPLEMENTATION [riscv && need_xcpu_tlb_flush]:

IMPLEMENT inline
void
Mem_space::sync_write_tlb_active_on_cpu()
{
  // Ensure that the write to _tlb_active_on_cpu (store) is visible to all other
  // CPUs.
  Mem::mp_mb();

  // However, the above barrier alone is not sufficient to enforce an order with
  // respect to implicit page tables accesses:
  // The implicit reads and writes that instruction execution causes to the page
  // tables "are ordinarily not ordered with respect to explicit loads and
  // stores". Only after "executing an SFENCE.VMA it is guaranteed that any
  // previous stores already visible to the current RISC-V hart are ordered
  // before certain implicit references by subsequent instructions in that hart
  // to the memory-management data structures" (see RISC-V Instruction Set
  // Manual: Privileged Architecture).

  // Only necessary in case we use ASIDs, because without ASIDs an SFENCE.VMA
  // will already be executed when the memory space is made current. In the ASID
  // case the overhead of an SFENCE.VMA should be manageable, because
  // sync_write_tlb_active_on_cpu() is only executed when the memory space
  // becomes active on a CPU where it previously wasn't, so the TLB is empty for
  // the ASID anyway. Even though it may seem contradictory to then execute a
  // "TLB flush" despite the TLB being empty, it is required because of the
  // ordering guarantees that the SFENCE.VMA instruction provides!
  if (Mem_unit::Have_asids)
    // Make sure to allocate an ASID for this memory space, if it does no yet
    // have one, as otherwise, when executed with an invalid ASID, the
    // SFENCE.VMA instruction would not affect this memory space. If a new ASID
    // was allocated, either because the memory space did not have one or an
    // ASID rollover took place, this ASID is now marked as the active ASID of
    // the current CPU in the ASID allocator. But this is not a problem, because
    // sync_write_tlb_active_on_cpu() or rather tlb_mark_used() is invoked
    // immediately before Mem_space::make_current() which makes this memory
    // space the current memory space on the current CPU anyway.
    Mem_unit::tlb_flush(asid());
}

IMPLEMENT inline
void
Mem_space::sync_read_tlb_active_on_cpu()
{
  // Ensure that all changed page table entries (store) are visible to all other
  // CPUs, before accessing _tlb_active_on_cpu (load) on the current CPU.
  Mem::mp_mb();
}
