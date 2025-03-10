INTERFACE [io]:

#include "types.h"
#include "config.h"
#include "kmem_slab.h"
#include "mem_space.h"

class Mem_space;
class Space;

/**
 * IO bitmap storage.
 *
 * This wraps the IO bitmap using a Bitmap_storage<> class for conveniently
 * acessing the IO bitmap bits.
 */
class Generic_io_bitmap :
  public Bitmap_storage<Config::Io_bitmap>
{};

/**
 * Wrapper class for io_{map,unmap}.
 *
 * This class serves as an adapter for map<Generic_io_space> to Mem_space.
 */
template<typename SPACE>
class Generic_io_space
{
  friend class Jdb_iomap;

public:
  static char const *const name;

  typedef Port_number V_pfn;
  typedef Port_number V_pfc;
  typedef Port_number Phys_addr;
  typedef Order Page_order;
  typedef Page::Rights Attr;

  enum
  {
    Need_insert_tlb_flush = 0,
    Need_upgrade_tlb_flush = 0,
    Need_xcpu_tlb_flush = 0,
    Map_page_size = 1,
    Page_shift = 0,
    Map_superpage_shift = 16,
    Map_superpage_size = 0x10000,
    Map_max_address = 0x10000,
    Whole_space = 16,
    Identity_map = 1,
  };

  static_assert(static_cast<unsigned int>(Map_superpage_size)
                == Config::Io_port_count);
  static_assert(static_cast<unsigned int>(Map_max_address)
                == Config::Io_port_count);
  static_assert((1U << Map_superpage_shift) == Config::Io_port_count);
  static_assert((1U << Whole_space) == Config::Io_port_count);

  struct Fit_size
  {
    Page_order operator ()(Page_order o) const
    {
      return o >= Page_order(Map_superpage_shift)
             ? Page_order(Map_superpage_shift)
             : Page_order(0);
    }
  };

  // We'd rather like to use a "using Mem_space::Status" declaration here,
  // but that wouldn't make the enum values accessible as
  // Generic_io_space::Insert_ok and so on.
  enum Status
  {
    Insert_ok = 0,              ///< Mapping was added successfully.
    Insert_warn_exists,         ///< Mapping already existed
    Insert_warn_attrib_upgrade, ///< Mapping already existed, attribs upgrade
    Insert_err_nomem,           ///< Couldn't alloc new page table
    Insert_err_exists           ///< A mapping already exists at the target addr
  };

  static V_pfn map_max_address()
  { return V_pfn(Map_max_address); }

  static Phys_addr page_address(Phys_addr o, Page_order s)
  { return cxx::mask_lsb(o, s); }

  static Phys_addr subpage_address(Phys_addr addr, V_pfc offset)
  { return addr | offset; }

  static V_pfc subpage_offset(V_pfn addr, Page_order size)
  { return cxx::get_lsb(addr, size); }

  static Mdb_types::Pfn to_pfn(V_pfn p)
  { return Mdb_types::Pfn(cxx::int_value<V_pfn>(p)); }

  static V_pfn to_virt(Mdb_types::Pfn p)
  { return V_pfn(cxx::int_value<Mdb_types::Pfn>(p)); }

  static Mdb_types::Pcnt to_pcnt(Page_order s)
  { return Mdb_types::Pcnt(cxx::int_value<V_pfc>(V_pfc(1) << s)); }

  static Page_order to_order(Mdb_types::Order p)
  { return Page_order(cxx::int_value<Mdb_types::Order>(p)); }

  static V_pfc to_size(Page_order p)
  { return V_pfc(1) << p; }


  FIASCO_SPACE_VIRTUAL
  Status v_insert(Phys_addr phys, V_pfn virt, Page_order size, Attr page_attribs);

  FIASCO_SPACE_VIRTUAL
  bool v_lookup(V_pfn virt, Phys_addr *phys = nullptr, Page_order *order = nullptr,
                Attr *attribs = nullptr);
  virtual
  bool v_fabricate(V_pfn address, Phys_addr *phys, Page_order *order,
                   Attr *attribs = nullptr);
  FIASCO_SPACE_VIRTUAL
  Page::Flags v_delete(V_pfn virt, Page_order order, Page::Rights rights);

private:
  /**
   * IO bitmap.
   *
   * The IO bitmap is dynamically allocated on demand. If this member is
   * nullptr, then the hypothetical IO bitmap is considered to contain
   * all bits set (i.e. all IO ports disabled).
   */
  Generic_io_bitmap *_bitmap;

  /**
   * IO bitmap revision.
   *
   * The revision is increased with every IO bitmap update. On overflow, the
   * value 0 is avoided to make sure that the comparison with the revision
   * number of the CPU IO bitmap is never a false positive.
   *
   * This means that only an unallocated IO bitmap has the revision of 0.
   */
  Mword _revision;

  /**
   * Number of enabled IO ports.
   */
  unsigned int _counter;

  /**
   * Indicate whether the IO space is represented as a single superpage
   * (i.e. all IO ports enabled).
   */
  bool _superpage;

  /**
   * IO bitmap activity tracking.
   *
   * Track on which CPUs this IO space is currently "active", i.e. on which
   * CPUs it could be potentially synchronized with the CPU IO bitmap.
   *
   * \note Each CPU bit must only be set from the corresponding CPU!
   */
  Cpu_mask _iopb_active_on_cpu;

  Mem_space const *mem_space() const
  { return static_cast<SPACE const *>(this); }

  Mem_space *mem_space()
  { return static_cast<SPACE *>(this); }

  /**
   * Mark this IO space as potentially "active" on the current CPU.
   */
  inline void iopb_mark_used()
  {
    _iopb_active_on_cpu.atomic_set_if_unset(current_cpu());
  }

  /**
   * Mark this IO space as not "active" on the current CPU.
   */
  inline void iopb_mark_unused()
  {
    _iopb_active_on_cpu.atomic_clear_if_set(current_cpu());
  }

  /**
   * Get the activity of this IO space on the current CPU.
   *
   * \note We assume that accessing the CPU mask is atomic.
   *
   * \return Activity state.
   */
  inline bool iopb_used()
  {
    return _iopb_active_on_cpu.get(current_cpu());
  }
};

template<typename SPACE>
char const *const Generic_io_space<SPACE>::name = "Io_space";

//----------------------------------------------------------------------------
IMPLEMENTATION [io]:

#include <cassert>
#include <cstring>

#include "atomic.h"
#include "config.h"
#include "l4_types.h"
#include "panic.h"
#include "paging.h"
#include "cpu_mask.h"
#include "cpu_call.h"

static Kmem_slab_t<Generic_io_bitmap> _io_bitmap_alloc("Io_bmap");

PUBLIC static
Generic_io_bitmap *
Generic_io_bitmap::alloc(Ram_quota *quota)
{
  return _io_bitmap_alloc.q_new(quota);
}

PUBLIC static
void
Generic_io_bitmap::free(Ram_quota *quota, Generic_io_bitmap *bitmap)
{
  _io_bitmap_alloc.q_del(quota, bitmap);
}

/**
 * IO bitmap storage constructor.
 *
 * All bits in the bitmap are set (i.e. all IO ports disabled).
 */
PUBLIC inline
Generic_io_bitmap::Generic_io_bitmap()
{
  set_all();
}

/**
 * Set all bits (i.e. all IO ports disabled)
 */
PUBLIC inline
void
Generic_io_bitmap::set_all()
{
  memset(_bits, 0xffU, size_in_bytes(Config::Io_port_count));
}

/**
 * Copy the IO bitmap storage to a different bitmap.
 *
 * \param[out] dest  Destination bitmap.
 */
PUBLIC inline
void
Generic_io_bitmap::copy_to(Config::Io_bitmap *dest)
{
  static_assert(sizeof(*dest) == sizeof(_bits));
  memcpy(*dest, _bits, sizeof(*dest));
}

PUBLIC template<typename SPACE>
inline
typename Generic_io_space<SPACE>::Fit_size
Generic_io_space<SPACE>::fitting_sizes() const
{
  return Fit_size();
}

PUBLIC template<typename SPACE>
static inline
bool
Generic_io_space<SPACE>::is_full_flush(L4_fpage::Rights rights)
{
  return static_cast<bool>(rights);
}

PUBLIC template<typename SPACE>
inline
Generic_io_space<SPACE>::Generic_io_space()
  : _bitmap(nullptr), _revision(0), _counter(0), _superpage(false)
{}

PUBLIC template<typename SPACE>
Generic_io_space<SPACE>::~Generic_io_space()
{
  if (_bitmap != nullptr)
    Generic_io_bitmap::free(ram_quota(), _bitmap);
}

PUBLIC template<typename SPACE>
inline
Ram_quota *
Generic_io_space<SPACE>::ram_quota() const
{
  return static_cast<SPACE const *>(this)->ram_quota();
}

//
// Utilities for map<Generic_io_space> and unmap<Generic_io_space>
//

IMPLEMENT template<typename SPACE>
bool
Generic_io_space<SPACE>::v_fabricate(V_pfn address, Phys_addr *phys,
                                     Page_order *order, Attr *attribs)
{
  return this->v_lookup(address, phys, order, attribs);
}

IMPLEMENT template<typename SPACE>
inline
bool FIASCO_FLATTEN
Generic_io_space<SPACE>::v_lookup(V_pfn virt, Phys_addr *phys,
                                  Page_order *order, Attr *attribs)
{
  if (_superpage)
    {
      /*
       * If the space is allocated as superpage, then all IO ports are enabled.
       */

      if (order) *order = Page_order(Map_superpage_shift);
      if (phys) *phys = Phys_addr(0);
      if (attribs) *attribs = Attr::URW();

      return true;
    }

  if (order) *order = Page_order(0);

  if (io_enabled(cxx::int_value<V_pfn>(virt)))
    {
      /*
       * IO port is explicitly enabled.
       */

      if (phys) *phys = virt;
      if (attribs) *attribs = Attr::URW();

      return true;
    }

  if (_counter == 0)
    {
      if (order) *order = Page_order(Map_superpage_shift);
      if (phys) *phys = Phys_addr(0);
    }

  return false;
}

IMPLEMENT template<typename SPACE>
inline
Page::Flags FIASCO_FLATTEN
Generic_io_space<SPACE>::v_delete(V_pfn virt, [[maybe_unused]] Page_order order,
                                  Page::Rights rights)
{
  if (!(rights & Page::Rights::FULL()))
    return Page::Flags::None();

  if (_superpage)
    {
      /*
       * If the space is allocated as superpage, then all IO ports are enabled.
       * We need to explicitly disable all IO ports.
       */

      assert(order == Page_order(Map_superpage_shift));

      for (Address p = 0; p < Map_max_address; ++p)
        io_disable(p);

      assert(_counter == 0);

      _superpage = false;

      /*
       * Flush the CPU IO bitmaps to make sure that no IO access is possible
       * via a stale entry.
       */
      io_bitmap_flush_all_cpus();

      return Page::Flags::None();
    }

  assert(order == Page_order(0));

  io_disable(cxx::int_value<V_pfn>(virt));

  /*
   * Flush the CPU IO bitmaps to make sure that no IO access is possible
   * via a stale entry.
   */
  io_bitmap_flush_all_cpus();

  return Page::Flags::None();
}

IMPLEMENT template<typename SPACE>
inline
typename Generic_io_space<SPACE>::Status FIASCO_FLATTEN
Generic_io_space<SPACE>::v_insert([[maybe_unused]] Phys_addr phys,
                                  V_pfn virt, Page_order size,
                                  Attr /* page_attribs */)
{
  assert(phys == virt);

  /*
   * If the space is allocated as superpage, then all IO ports are
   * already enabled.
   */
  if (_superpage && size == Page_order(Map_superpage_shift))
    return Insert_warn_exists;

  if (_counter == 0 && size == Page_order(Map_superpage_shift))
    {
      /*
       * Enable all IO ports.
       */
      for (Address p = 0; p < Map_max_address; ++p)
        io_enable(p);

      _superpage = true;
      return Insert_ok;
    }

  assert(size == Page_order(0));

  return typename Generic_io_space::Status(io_enable(cxx::int_value<V_pfn>(virt)));
}

PUBLIC template<typename SPACE>
inline static
void
Generic_io_space<SPACE>::tlb_flush()
{}

//
// I/O port lookup / insert / delete / counting
//

/** Get I/O port status in the I/O space.
 *
 * \param port  Port number to examine.
 *
 * \retval True if port is enabled.
 * \retval False if port is disabled.
 */
PROTECTED template<typename SPACE>
inline
bool
Generic_io_space<SPACE>::io_enabled(Address port) const
{
  assert(port < Config::Io_port_count);

  /* No bitmap means no port mapped. */
  if (_bitmap == nullptr)
    return false;

  bool bit = (*_bitmap)[port];

  /*
   * The bitmap logic is unintuitively reversed to mimick the hardware usage.
   *
   * bit == 1 indicates a disabled port
   * bit == 0 indicates an enabled port
   */

  return !bit;
}

/** Enable I/O port in the I/O space.
 *
 * This also increments the IO bitmap revision to make sure it is synchronized
 * with the respective CPU IO bitmap.
 *
 * This function is called in the context of the IPC sender.
 *
 * \param port  Port number to enable.
 *
 * \retval Insert_ok on success
 * \retval Insert_warn_exists if the port is already enabled.
 * \retval Insert_err_nomem if memory allocation failed.
 */
PROTECTED template<typename SPACE>
inline
typename Generic_io_space<SPACE>::Status
Generic_io_space<SPACE>::io_enable(Address port)
{
  assert(port < Config::Io_port_count);

  if (_bitmap == nullptr)
    {
      _bitmap = Generic_io_bitmap::alloc(ram_quota());
      if (_bitmap == nullptr)
        return Insert_err_nomem;
    }

  bool bit = (*_bitmap)[port];

  /*
   * The bitmap logic is unintuitively reversed to mimick the hardware usage.
   *
   * bit == 1 indicates a disabled port
   * bit == 0 indicates an enabled port
   */

  if (bit)
    {
      _bitmap->clear_bit(port);
      ++_counter;

      /*
       * This forces the IO bitmap to be copied to the respective CPU
       * IO bitmap in case of an IO port access exception.
       */
      ++_revision;

      /*
       * To avoid a potential (unlikely, but possible) false positive
       * comparison with the CPU IO bitmap revision, the value 0 is skipped
       * in case revision overflow.
       */
      if (_revision == 0)
        ++_revision;

      return Insert_ok;
    }

  /* Port is already enabled. */
  return Insert_warn_exists;
}

/** Disable I/O port in the I/O space.
 *
 * \note The IO bitmap revision is unchanged. Therefore it is expected
 *       that the caller flushes the CPU IO bitmaps to make sure they are
 *       resynchronized with the new state of the IO bitmap.
 *
 * \param port  Port number to disable.
 */
PROTECTED template<typename SPACE>
inline
void
Generic_io_space<SPACE>::io_disable(Address port)
{
  assert(port < Config::Io_port_count);

  /* If there is no bitmap, then all ports are implicitly disabled. */
  if (_bitmap == nullptr)
    return;

  bool bit = (*_bitmap)[port];

  /*
   * The bitmap logic is unintuitively reversed to mimick the hardware usage.
   *
   * bit == 1 indicates a disabled port
   * bit == 0 indicates an enabled port
   */

  if (!bit)
    {
      assert(_counter > 0);

      _bitmap->set_bit(port);
      --_counter;
    }
}

PUBLIC template<typename SPACE>
inline static
typename Generic_io_space<SPACE>::V_pfn
Generic_io_space<SPACE>::canonize(V_pfn v)
{ return v; }

/**
 * Update the current CPU IO bitmap.
 *
 * In case of the IO bitmap revision not being equal to the CPU IO bitmap
 * revision, the contents of the IO bitmap is copied to the CPU IO bitmap
 * and the CPU IO bitmap revision is updated.
 *
 * \note It is assumed that the arguments of this method point to the CPU IO
 *       bitmap of the current CPU and that the CPU lock is held while calling
 *       this method.
 *
 * \param[io,out] dest_revision  Current CPU IO bitmap revision.
 * \param[out]    dest_bitmap    Current CPU IO bitmap.
 *
 * \retval false  The CPU IO bitmap was not updated.
 * \retval true   The CPU IO bitmap was updated.
 */
PUBLIC template<typename SPACE>
inline
bool
Generic_io_space<SPACE>::update_io_bitmap(Mword *dest_revision,
                                          Config::Io_bitmap *dest_bitmap)
{
  if (*dest_revision != _revision)
    {
      assert(_revision != 0);
      assert(_bitmap != nullptr);

      _bitmap->copy_to(dest_bitmap);
      *dest_revision = _revision;
      iopb_mark_used();

      return true;
    }

  return false;
}

/**
 * Flush the IO bitmaps of all CPUs where this IO space is "active".
 *
 * This method needs to be called whenever a port is disabled in an IO space.
 *
 * To achieve the flush, we reset the CPU IO bitmaps of the affected CPUs.
 * This forces the CPU IO bitmaps to be resynchronized with any given IO space
 * on the next IO port access.
 *
 * To avoid locking the IO bitmaps, we use IPI to reset the IO bitmaps locally
 * on every affected CPU, similarily to a TLB flush. We assume that disabling
 * an IO port is not a frequent operation. The overhead only affects the CPUs
 * where the IO space is "active".
 *
 * \pre The cpu lock must not be held (because of cross-CPU call).
 */
PRIVATE template<typename SPACE>
inline
void
Generic_io_space<SPACE>::io_bitmap_flush_all_cpus()
{
  Cpu_call::cpu_call_many(_iopb_active_on_cpu, [this](Cpu_number)
    {
      Cpu &cpu = Cpu::cpus.current();
      cpu.reset_io_bitmap();
      iopb_mark_unused();
      return false;
    });
}

/**
 * Activation of the current IO space.
 *
 * If the current IO space differs from the previous IO space and the previous
 * IO space is marked as "active" on the current CPU, the CPU IO bitmap is
 * reset (to force a resynchronization on the next IO instruction) and the
 * previous IO space is marked as not "active" on the current CPU.
 *
 * \param from  Previous IO space.
 */
PUBLIC template<typename SPACE>
inline
void
Generic_io_space<SPACE>::switchin_context(Generic_io_space<SPACE> *from)
{
  if (from != this && from->iopb_used())
    {
      /* Make sure the CPU IO bitmap is pristine. */
      Cpu &cpu = Cpu::cpus.current();
      cpu.reset_io_bitmap();

      from->iopb_mark_unused();
    }
}
