INTERFACE:

#include "auto_quota.h"
#include "cpu_mask.h"
#include "paging.h"		// for page attributes
#include "mem_layout.h"
#include "member_offs.h"
#include "per_cpu_data.h"
#include "ram_quota.h"
#include "types.h"
#include "mapdb_types.h"

/**
 * An address space.
 *
 * Insertion and lookup functions.
 */
class Mem_space
{
  MEMBER_OFFSET();

public:
  typedef int Status;
  static char const *const name;

  typedef Page::Attr Attr;
  typedef Pdir::Va Vaddr;
  typedef Pdir::Vs Vsize;

  typedef Addr::Addr<Config::PAGE_SHIFT> Phys_addr;
  typedef Addr::Diff<Config::PAGE_SHIFT> Phys_diff;
  typedef Addr::Order<Config::PAGE_SHIFT> Page_order;

  // for map_util
  typedef Page_number V_pfn;
  typedef Page_count V_pfc;
  typedef Addr::Order<0> V_order;

  enum Switchin_flags
  {
    None = 0,
    Vcpu_user_to_kern = 1,
  };

  // Each architecture must provide these members:
  void switchin_context(Mem_space *from, Switchin_flags flags);

  enum Tlb_type
  {
    /* CPU-TLB with ASIDs, like Arm-v7, or x86-with-PCID */
    Tlb_per_cpu_asid,

    /* CPU-TLB that flushes when switching spaces, like x86-without-PCID, or
     * ARM-v5 */
    Tlb_per_cpu_global,

    /* IOMMU-TLB, assumed to be global, only needing a single flush to be
       effective everywhere */
    Tlb_iommu,
  };

  /**
   * Returns the TLB type of this memory space.
   */
  Tlb_type tlb_type() const
  { return _tlb_type; }

  /**
   * Local TLB flush, i.e. only on the current CPU.
   *
   * Flushes all entries of this memory space from the TLB.
   *
   * @note Might also affect the TLB on other CPUs, if the platform provides a
   *       mechanism for global TLB invalidation.
   */
  FIASCO_SPACE_VIRTUAL
  void tlb_flush_current_cpu();

  /**
   * TLB flush on all CPUs where the memory space is marked as "active".
   *
   * Flushes all entries of this memory space from the TLB.
   *
   * @pre The cpu lock must not be held (because of cross-CPU call).
   *
   * @note Depenending on the TLB type and usage of this memory space, and if
   *       the platform provides no mechanisms for global TLB invalidation, a
   *       cross-CPU TLB flush can be a very costly operation (might need an
   *       IPI-coordinated TLB shootdown).
   */
  void tlb_flush_all_cpus();

  /** Insert a page-table entry, or upgrade an existing entry with new
   *  attributes.
   *
   * @param phys  Physical address.
   * @param virt  Virtual address for which an entry should be created.
   * @param order  log2 of the page frame size.
   * @param page_attribs  Attributes for the mapping (see Page::Attr).
   * @param ku_mem        Is it a kernel-user memory mapping?
   * @return Insert_ok if a new mapping was created;
   *         Insert_warn_exists if the mapping already exists;
   *         Insert_warn_attrib_upgrade if the mapping already existed but
   *                                    attributes could be upgraded;
   *         Insert_err_nomem if the mapping could not be inserted because
   *                          the kernel is out of memory;
   *         Insert_err_exists if the mapping could not be inserted because
   *                           another mapping occupies the virtual-address
   *                           range
   * @pre Not thread-safe, the caller must ensure that no one else modifies the
   *      page table at the same time.
   * @pre phys and virt need to be aligned according to the size argument.
   * @pre size must match one of the frame sizes used in the page table.
   *      See fitting_sizes().
   */
  FIASCO_SPACE_VIRTUAL
  Status v_insert(Phys_addr phys, Vaddr virt, Page_order order,
                  Attr page_attribs, bool ku_mem = false);

  /** Look up a page-table entry.
   *
   * @param virt  Virtual address for which we try the lookup.
   * @param[out] phys  Meaningful only if we find something (and return true).
   *              If not 0, we fill in the physical address of the found page
   *              frame.
   * @param[out] order  If not 0, we fill in the size of the page-table slot.
   *              If an entry was found (and we return true), this is log2 of
   *              the size of the page frame.  If no entry was found (and we
   *              return false), this is the size of the free slot.  In either
   *              case, it is equal to one of the frame sizes used in the page
   *              table. See fitting_sizes().
   * @param[out] page_attribs  Meaningful only if we find something (and return
   *              true). If not 0, we fill in the page attributes for the
   *              found page frame (see Page::Attr).
   * @return True if an entry was found, false otherwise.
   */
  FIASCO_SPACE_VIRTUAL
  bool v_lookup(Vaddr virt, Phys_addr *phys = nullptr, Page_order *order = nullptr,
                Attr *page_attribs = nullptr);

  /** Invalidate page-table entries, or some of the entries' attributes.
   *
   * @param virt    Virtual address of the memory region that should be
   *                changed.
   * @param order   Order of the memory region that should be changed.
   * @param rights  Revoke only the given page rights (bit-ORed, see
   *                #Page::Rights). If #Page::Rights::R() is part of the
   *                bitmask, the entry is invalidated.
   *
   * @retval #Page::Flags::None()  The entry is already invalid or the page
   *         access flags were unset before the entry was touched (by this
   *         function), or page access flags are not supported.
   * @retval otherwise  Combined (bit-ORed) page access flags of the entry
   *         before it was modified. Support for this information is
   *         platform-dependent.
   *
   * @pre Not thread-safe, the caller must ensure that no one else modifies the
   *      page table at the same time.
   * @pre `virt` needs to be aligned according to the size argument.
   * @pre `order` must match one of the frame sizes used in the page table.
   *      See fitting_sizes().
   *
   * @note No memory is freed.
   */
  FIASCO_SPACE_VIRTUAL
  Page::Flags v_delete(Vaddr virt, Page_order order, Page::Rights rights);

  /**
   * Amend the page access flags on platforms where this feature is supported.
   *
   * The method does not unset access flags which are already set.
   *
   * @param virt   Virtual address of the affected memory region.
   * @param flags  #Page::Flags::Referenced(): page was referenced.
   *               #Page::Flags::Dirty(): page is dirty.
   *
   * @pre Not thread-safe, the caller must ensure that no one else modifies the
   *      page table at the same time.
   *
   * @note Support for setting the page access flags is platform-dependent.
   *       If this feature is not supported, this function does nothing.
   */
  FIASCO_SPACE_VIRTUAL
  void v_add_access_flags(Vaddr virt, Page::Flags flags);

  /** Set this memory space as the current on this CPU. */
  void make_current(Switchin_flags flags = None);

  /**
   * Reload current Mem_space state on this CPU.
   *
   * This is only needed on MPU systems. On MMU systems the hardware does
   * the page walk independently (after TLB was evicted).
   */
  static void reload_current();

  static Mem_space *kernel_space()
  { return _kernel_space; }

  /** Return the current memory space of this CPU. */
  static inline Mem_space *current_mem_space(Cpu_number cpu)
  { return _current.cpu(cpu); }

  /**
   * Translate virtual address located in pmem to physical address.
   *
   * @param virt  Virtual address located in pmem.
   *              This address does not need to be page-aligned.
   *
   * @return Physical address corresponding to virt.
   */
  Address pmem_to_phys(Address virt) const;

/**
 * Simple page-table lookup.
 *
 * This method is similar to virt_to_phys(), with the difference that this
 * version handles Sigma0's address space as a special case (by having the
 * Sigma0 task class provide a specialized override of this method): For Sigma0,
 * we do not actually consult the page table, as it is meaningless because we
 * create new mappings for Sigma0 transparently; instead, we return the
 * logically-correct result of physical address == virtual address.
 *
 * @param virt Virtual address. This address does not need to be page-aligned.
 * @return Physical address corresponding to virt.
 */
  virtual
  Address virt_to_phys_s0(void *virt) const
  { return virt_to_phys(reinterpret_cast<Address>(virt)); }


  virtual
  Page_number mem_space_map_max_address() const
  { return Page_number(Virt_addr(Mem_layout::User_max)) + Page_count(1); }

  Page_number map_max_address() const
  { return mem_space_map_max_address(); }

  static Phys_addr page_address(Phys_addr o, Page_order s)
  { return cxx::mask_lsb(o, s); }

  static V_pfn page_address(V_pfn a, Page_order o)
  { return cxx::mask_lsb(a, o); }

  static Phys_addr subpage_address(Phys_addr addr, V_pfc offset)
  { return addr | Phys_diff(offset); }

  struct Fit_size
  {
    typedef cxx::array<Page_order, Page_order, 65> Size_array;
    Size_array o;
    Page_order operator () (Page_order i) const { return o[i]; }

    void add_page_size(Page_order order)
    {
      for (Page_order c = order; c < o.size() && o[c] < order; ++c)
        o[c] = order;
    }
  };

  FIASCO_SPACE_VIRTUAL
  Fit_size const &mem_space_fitting_sizes() const __attribute__((pure));

  Fit_size const &fitting_sizes() const
  { return mem_space_fitting_sizes(); }

  static Mdb_types::Pfn to_pfn(Phys_addr p)
  { return Mdb_types::Pfn(cxx::int_value<Page_number>(p)); }

  static Mdb_types::Pfn to_pfn(V_pfn p)
  { return Mdb_types::Pfn(cxx::int_value<Page_number>(p)); }

  static Mdb_types::Pcnt to_pcnt(Page_order s)
  { return Mdb_types::Pcnt(1) << Mdb_types::Order(cxx::int_value<Page_order>(s) - Config::PAGE_SHIFT); }

  static V_pfn to_virt(Mdb_types::Pfn p)
  { return Page_number(cxx::int_value<Mdb_types::Pfn>(p)); }

  static Page_order to_order(Mdb_types::Order p)
  { return Page_order(cxx::int_value<Mdb_types::Order>(p) + Config::PAGE_SHIFT); }

  static V_pfc to_size(Page_order p)
  { return V_pfc(1) << p; }

  static V_pfc subpage_offset(V_pfn a, Page_order o)
  { return cxx::get_lsb(a, o); }

  Page_order largest_page_size() const
  { return mem_space_fitting_sizes()(Page_order(64)); }

  enum
  {
    Max_num_global_page_sizes = 5
  };

  static Page_order const *get_global_page_sizes(bool finalize = true)
  {
    if (finalize)
      _glbl_page_sizes_finished = true;
    return _glbl_page_sizes;
  }

protected:
  /**
   * TLB type of this memory space.
   *
   * The TLB type is typically a compile-time constant per Mem_space
   * implementation, so initially this is set to the TLB type of regular address
   * spaces.
   * Mem_space overrides, such as VM address spaces or DMA address spaces, might
   * set a different TLB type in their constructor.
   */
  Tlb_type _tlb_type = regular_tlb_type();

private:
  /**
   * Return TLB type of regular address spaces.
   */
  static Tlb_type regular_tlb_type();

  Mem_space(const Mem_space &) = delete;

  Ram_quota *_quota;

  static Per_cpu<Mem_space *> _current;
  static Global_data<Mem_space *> _kernel_space;
  static Global_data<Page_order[Max_num_global_page_sizes]> _glbl_page_sizes;
  static Global_data<unsigned> _num_glbl_page_sizes;
  static Global_data<bool> _glbl_page_sizes_finished;
};

//---------------------------------------------------------------------------
INTERFACE [need_xcpu_tlb_flush]:

EXTENSION class Mem_space
{
public:
  /**
   * The option need_xcpu_tlb_flush indicates that memory spaces with a TLB
   * type of Tlb_per_cpu_asid or Tlb_per_cpu_global need cross-CPU TLB flushes
   * coordinated via IPIs.
   *
   * To avoid sending IPIs to all cores, track for each memory space the cores
   * it might have TLB entries, which then allows to limit sending of IPIs to
   * these cores.
   */
  enum { Need_xcpu_tlb_flush = 1 };

  Cpu_mask const &tlb_active_on_cpu() const
  { return _tlb_active_on_cpu; }

  /**
   * Ensure that setting the active bit in _tlb_active_on_cpu for a memory space
   * on the current CPU is visible to all other CPUs, before any page table
   * entry of the memory space is accessed on the current CPU, thus potentially
   * cached in the TLB.
   */
  void sync_write_tlb_active_on_cpu();

  /**
   * Ensure that all changes to page tables made on the current CPU are visible
   * to all other CPUs, before accessing _tlb_active_on_cpu on the current CPU.
   */
  static void sync_read_tlb_active_on_cpu();

protected:
  /**
   * Mark this memory space as potentially having TLB entries on the
   * current CPU.
   */
  inline void tlb_mark_used()
  {
    if (!_tlb_active_on_cpu.atomic_get_and_set_if_unset(current_cpu()))
      // If the memory space was not already marked as active, we have to
      // execute a synchronization operation.
      sync_write_tlb_active_on_cpu();
  }

  /**
   * Mark this memory space as having no TLB entries on the current CPU.
   */
  inline void tlb_mark_unused()
  { _tlb_active_on_cpu.atomic_clear_if_set(current_cpu()); }

private:
  /**
   * Mark this memory space as having no TLB entries on the current CPU if it
   * is not the current memory space on the CPU.
   */
  inline void tlb_mark_unused_if_non_current()
  {
    if (_current.current() != this)
      tlb_mark_unused();
  }

  /**
   * Track on which CPUs this memory space is currently "active", i.e. on which
   * CPUs it could potentially have TLB entries. Each CPU bit must only be set
   * from the corresponding CPU!
   *
   * TODO: this could be pretty CL-bouncing. Is it worth having a per-cpu-style
   * mechanisms for bits/booleans?
   */
  Cpu_mask _tlb_active_on_cpu;

  static Cpu_mask _tlb_active;
};

//---------------------------------------------------------------------------
INTERFACE [!need_xcpu_tlb_flush]:

EXTENSION class Mem_space
{
public:
  /**
   * This is the case where no IPI-based cross-CPU TLB flushes are needed,
   * e.g. because multi processor support is not enabled in the kernel
   * configuration or the platform provides mechanisms for global TLB
   * invalidation without IPIs.
   */
  enum { Need_xcpu_tlb_flush = 0 };

  Cpu_mask tlb_active_on_cpu() const { return Cpu_mask(); }
  static void sync_write_tlb_active_on_cpu() {};
  static void sync_read_tlb_active_on_cpu() {};

protected:
  void tlb_mark_used() {};
  void tlb_mark_unused() {};

private:
  void tlb_mark_unused_if_non_current() {};
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

//
// class Mem_space
//

#include "config.h"
#include "cpu_call.h"
#include "globals.h"
#include "l4_types.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "paging.h"
#include "panic.h"
#include "global_data.h"

DEFINE_PER_CPU Per_cpu<Mem_space *> Mem_space::_current;


char const * const Mem_space::name = "Mem_space";
DEFINE_GLOBAL Global_data<Mem_space *> Mem_space::_kernel_space;

static DEFINE_GLOBAL Global_data<Mem_space::Fit_size> __mfs;

DEFINE_GLOBAL
Global_data<Mem_space::Page_order[Mem_space::Max_num_global_page_sizes]>
Mem_space::_glbl_page_sizes;

DEFINE_GLOBAL Global_data<unsigned> Mem_space::_num_glbl_page_sizes;
DEFINE_GLOBAL Global_data<bool> Mem_space::_glbl_page_sizes_finished;

PROTECTED static
void
Mem_space::add_global_page_size(Page_order o)
{
  assert (!_glbl_page_sizes_finished);
  unsigned i;
  for (i = 0; i < _num_glbl_page_sizes; ++i)
    {
      if (_glbl_page_sizes[i] == o)
        return;

      if (_glbl_page_sizes[i] < o)
        break;
    }

  assert (_num_glbl_page_sizes + 1 < Max_num_global_page_sizes);

  for (unsigned x = _num_glbl_page_sizes; x > i; --x)
    _glbl_page_sizes[x] = _glbl_page_sizes[x - 1];

  _glbl_page_sizes[i] = o;
  assert (_glbl_page_sizes[_num_glbl_page_sizes] <= Page_order(Config::PAGE_SHIFT));

  ++_num_glbl_page_sizes;
}

PUBLIC static
void
Mem_space::add_page_size(Page_order o)
{
  add_global_page_size(o);
  __mfs->add_page_size(o);
}

IMPLEMENT
Mem_space::Fit_size const &
Mem_space::mem_space_fitting_sizes() const
{
  return __mfs;
}

PUBLIC inline
Ram_quota *
Mem_space::ram_quota() const
{ return _quota; }


PUBLIC inline
Mem_space::Dir_type*
Mem_space::dir ()
{ return _dir; }

PUBLIC inline
const Mem_space::Dir_type*
Mem_space::dir() const
{ return _dir; }

PUBLIC
virtual bool
Mem_space::v_fabricate(Vaddr address, Phys_addr *phys, Page_order *order,
                       Attr *attribs = nullptr)
{
  return v_lookup(cxx::mask_lsb(address, Page_order(Config::PAGE_SHIFT)),
      phys, order, attribs);
}

PUBLIC virtual
bool
Mem_space::is_sigma0() const
{ return false; }

IMPLEMENT_DEFAULT inline NEEDS["kmem.h", "logdefs.h",
                               Mem_space::tlb_track_space_usage]
void
Mem_space::switchin_context(Mem_space *from, Switchin_flags flags)
{
  // FIXME: this optimization breaks SMP task deletion, an idle thread
  // may run on an already deleted page table
#if 0
  // never switch to kernel space (context of the idle thread)
  if (this == kernel_space())
    return;
#endif

  if (from != this)
    {
      // Kernel-space never needs TLB flushes.
      if (this != kernel_space())
        tlb_mark_used();

      CNT_ADDR_SPACE_SWITCH;
      make_current(flags);

      from->tlb_track_space_usage();
    }
}

IMPLEMENT_DEFAULT inline
Address
Mem_space::pmem_to_phys(Address virt) const
{ return Mem_layout::pmem_to_phys(virt); }

IMPLEMENT inline NEEDS["cpu_call.h"]
void
Mem_space::tlb_flush_all_cpus()
{
  if (!Mem_space::Need_xcpu_tlb_flush || tlb_type() == Mem_space::Tlb_iommu)
    {
      tlb_flush_current_cpu();
      return;
    }

  // To prevent a race condition that could potentially lead to the use of
  // outdated page table entries on other cores, we have to execute a memory
  // barrier that ensures that our PTE changes are visible to all other cores,
  // before we access tlb_active_on_cpu(). Otherwise, if the Mem_space gets
  // active on another core, shortly after we read tlb_active_on_cpu() where it
  // was reported as non-active, we won't send a TLB flush to the other core,
  // but it might not yet see our PTE changes.
  Mem_space::sync_read_tlb_active_on_cpu();

  Cpu_call::cpu_call_many(tlb_active_on_cpu(), [this](Cpu_number)
    {
      tlb_flush_current_cpu();
      return false;
    });
}

IMPLEMENT_DEFAULT static inline
void
Mem_space::reload_current()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [!need_xcpu_tlb_flush]:

PUBLIC static inline
void
Mem_space::enable_tlb(Cpu_number)
{}

PUBLIC static inline
void
Mem_space::disable_tlb(Cpu_number)
{}

PUBLIC static inline
Cpu_mask
Mem_space::active_tlb()
{ return Cpu_mask(); }

PRIVATE inline
void
Mem_space::tlb_track_space_usage()
{}

// ----------------------------------------------------------
IMPLEMENTATION [need_xcpu_tlb_flush]:

Cpu_mask Mem_space::_tlb_active;

PUBLIC static inline
Cpu_mask const &
Mem_space::active_tlb()
{ return _tlb_active; }

PUBLIC static inline
void
Mem_space::enable_tlb(Cpu_number cpu)
{ _tlb_active.atomic_set(cpu); }

PUBLIC static inline
void
Mem_space::disable_tlb(Cpu_number cpu)
{ _tlb_active.atomic_clear(cpu); }

/**
 * Update the TLB usage state of this memory space when switching away from it.
 */
PRIVATE inline
void
Mem_space::tlb_track_space_usage()
{
  // Use regular_tlb_type() instead of tlb_type(), to allow for compile-time
  // optimization of the following branch conditions. This is safe,
  // as tlb_track_space_usage() must and can only be invoked from a regular
  // Mem_space, which can be assigned as the current Mem_space on a CPU
  // (_current).
  // Mem_space overrides like Dmar_space or Vm_vmx_ept have to implement their
  // own TLB usage tracking mechanism.
  if (regular_tlb_type() == Tlb_per_cpu_global)
    {
      /* Without ASIDs, when we switched away from this space, the TLB
       * will have forgotten us. */
      assert(_current.current() != this);
      tlb_mark_unused();
    }
  else if (regular_tlb_type() == Tlb_per_cpu_asid)
    {
      /* With ASIDs the TLB has data until we flush it explicitly,
       * thus _tlb_active_on_cpu needs to keep its info */
      assert(_current.current() != this);
    }
}
