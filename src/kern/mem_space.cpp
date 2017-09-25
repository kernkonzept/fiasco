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

class Space;

/**
 * An address space.
 *
 * Insertion and lookup functions.
 */
class Mem_space
{
  MEMBER_OFFSET();

  // Space reverse lookup
  friend inline Mem_space* current_mem_space();

public:
  typedef int Status;
  static char const *const name;

  typedef Page::Attr Attr;
  typedef Pdir::Va Vaddr;
  typedef Pdir::Vs Vsize;

  typedef Addr::Addr<Config::PAGE_SHIFT> Phys_addr;
  typedef Addr::Diff<Config::PAGE_SHIFT> Phys_diff;
  typedef Addr::Order<Config::PAGE_SHIFT> Page_order;

  typedef void Reap_list;

  // for map_util
  typedef Page_number V_pfn;
  typedef Page_count V_pfc;
  typedef Addr::Order<0> V_order;

  // Each architecture must provide these members:
  void switchin_context(Mem_space *from);

  FIASCO_SPACE_VIRTUAL
  void tlb_flush(bool);

  /** Insert a page-table entry, or upgrade an existing entry with new
   *  attributes.
   *
   * @param phys Physical address (page-aligned).
   * @param virt Virtual address for which an entry should be created.
   * @param size Size of the page frame -- 4KB or 4MB.
   * @param page_attribs Attributes for the mapping (see
   *                     Mem_space::Page_attrib).
   * @return Insert_ok if a new mapping was created;
   *         Insert_warn_exists if the mapping already exists;
   *         Insert_warn_attrib_upgrade if the mapping already existed but
   *                                    attributes could be upgraded;
   *         Insert_err_nomem if the mapping could not be inserted because
   *                          the kernel is out of memory;
   *         Insert_err_exists if the mapping could not be inserted because
   *                           another mapping occupies the virtual-address
   *                           range
   * @pre phys and virt need to be size-aligned according to the size argument.
   */
  FIASCO_SPACE_VIRTUAL
  Status v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                  Attr page_attribs);

  /** Look up a page-table entry.
   *
   * @param virt Virtual address for which we try the look up.
   * @param phys Meaningful only if we find something (and return true).
   *             If not 0, we fill in the physical address of the found page
   *             frame.
   * @param page_attribs Meaningful only if we find something (and return
   *             true). If not 0, we fill in the page attributes for the
   *             found page frame (see Mem_space::Page_attrib).
   * @param size If not 0, we fill in the size of the page-table slot.  If an
   *             entry was found (and we return true), this is the size
   *             of the page frame.  If no entry was found (and we
   *             return false), this is the size of the free slot.  In
   *             either case, it is either 4KB or 4MB.
   * @return True if an entry was found, false otherwise.
   */
  FIASCO_SPACE_VIRTUAL
  bool v_lookup(Vaddr virt, Phys_addr *phys = 0, Page_order *order = 0,
                Attr *page_attribs = 0);

  /** Delete page-table entries, or some of the entries' attributes.
   *
   * This function works for one or multiple mappings (in contrast to
   * v_insert!).
   *
   * @param virt Virtual address of the memory region that should be changed.
   * @param size Size of the memory region that should be changed.
   * @param page_attribs If nonzero, delete only the given page attributes.
   *                     Otherwise, delete the whole entries.
   * @return Combined (bit-ORed) page attributes that were removed.  In
   *         case of errors, ~Page_all_attribs is additionally bit-ORed in.
   */
  FIASCO_SPACE_VIRTUAL
  L4_fpage::Rights v_delete(Vaddr virt, Page_order size,
                            L4_fpage::Rights page_attribs);

  FIASCO_SPACE_VIRTUAL
  void v_set_access_flags(Vaddr virt, L4_fpage::Rights access_flags);

  /** Set this memory space as the current on this CPU. */
  void make_current();

  static Mem_space *kernel_space()
  { return _kernel_space; }

  static inline Mem_space *current_mem_space(Cpu_number cpu);


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
    Size_array const &o;
    Page_order operator () (Page_order i) const { return o[i]; }

    explicit Fit_size(Size_array const &o) :o(o) {}
  };

  FIASCO_SPACE_VIRTUAL
  Fit_size mem_space_fitting_sizes() const __attribute__((pure));

  Fit_size fitting_sizes() const
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

private:
  Mem_space(const Mem_space &) = delete;

  Ram_quota *_quota;

  static Per_cpu<Mem_space *> _current;
  static Mem_space *_kernel_space;
  static Page_order _glbl_page_sizes[Max_num_global_page_sizes];
  static unsigned _num_glbl_page_sizes;
  static bool _glbl_page_sizes_finished;
};


//---------------------------------------------------------------------------
INTERFACE [mp]:

EXTENSION class Mem_space
{
public:
  enum { Need_xcpu_tlb_flush = true };

private:
  static Cpu_mask _tlb_active;
};



//---------------------------------------------------------------------------
INTERFACE [!mp]:

EXTENSION class Mem_space
{
public:
  enum { Need_xcpu_tlb_flush = false };
};


//---------------------------------------------------------------------------
IMPLEMENTATION:

//
// class Mem_space
//

#include "config.h"
#include "globals.h"
#include "l4_types.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "paging.h"
#include "panic.h"

DEFINE_PER_CPU Per_cpu<Mem_space *> Mem_space::_current;


char const * const Mem_space::name = "Mem_space";
Mem_space *Mem_space::_kernel_space;

static Mem_space::Fit_size::Size_array __mfs;
Mem_space::Page_order Mem_space::_glbl_page_sizes[Max_num_global_page_sizes];
unsigned Mem_space::_num_glbl_page_sizes;
bool Mem_space::_glbl_page_sizes_finished;

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
  for (Page_order c = o; c < __mfs.size(); ++c)
    __mfs[c] = o;
}

IMPLEMENT
Mem_space::Fit_size
Mem_space::mem_space_fitting_sizes() const
{
  return Fit_size(__mfs);
}

PUBLIC inline
Ram_quota *
Mem_space::ram_quota() const
{ return _quota; }


/// Avoid deallocation of page table upon Mem_space destruction.
PUBLIC
void
Mem_space::reset_dirty ()
{ _dir = 0; }

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
                       Attr *attribs = 0)
{
  return v_lookup(cxx::mask_lsb(address, Page_order(Config::PAGE_SHIFT)),
      phys, order, attribs);
}

PUBLIC virtual
bool
Mem_space::is_sigma0() const
{ return false; }

//---------------------------------------------------------------------------
IMPLEMENTATION [!io]:

PUBLIC inline
Mword
Mem_space::get_io_counter (void)
{ return 0; }

PUBLIC inline
bool
Mem_space::io_lookup (Address)
{ return false; }

//----------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

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
{
  Cpu_mask c;
  c.set(Cpu_number::boot_cpu());
  return c;
}

// ----------------------------------------------------------
IMPLEMENTATION [mp]:

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


