INTERFACE [ppc32]:

#include "entry_frame.h"

extern "C"
Mword
pagefault_entry(Address, Mword, Mword, Return_frame *);

EXTENSION class Mem_space
{
public:

  typedef Pdir Dir_type;

  /** Return status of v_insert. */
  enum //Status
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
      //Page_writable = Pt_entry::Writable,
      Page_cacheable = 0,
      /// Page is noncacheable.
      //Page_noncacheable = Pt_entry::Noncacheable | Pt_entry::Write_through,
      /// it's a user page.
      //Page_user_accessible = Pt_entry::User,
      /// Page has been referenced
      //Page_referenced = Pt_entry::Referenced,
      /// Page is dirty
      //Page_dirty = Pt_entry::Dirty,
      //Page_references = Page_referenced | Page_dirty,
      /// A mask which contains all mask bits
      //Page_all_attribs = Page_writable | Page_noncacheable |
//			 Page_user_accessible | Page_referenced | Page_dirty,
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

  bool try_htab_fault(Address virt);
protected:
  // DATA
  Dir_type *_dir;
};


//----------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include <cstring>
#include <cstdio>
#include "cpu.h"
#include "l4_types.h"
#include "mem_layout.h"
#include "paging.h"
#include "std_macros.h"
#include "kmem.h"
#include "logdefs.h"
#include "panic.h"
#include "lock_guard.h"
#include "cpu_lock.h"
#include "warn.h"




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
  //check//_dir->clear();	// initialize to zero
  return true; // success
}

PROTECTED inline
int
Mem_space::sync_kernel()
{ return 0; }

PUBLIC
Mem_space::Mem_space(Ram_quota *q, Dir_type* pdir)
  : _quota(q), _dir(pdir)
{
  _kernel_space = this;
  _current.cpu(Cpu_number::boot_cpu()) = this;
}

#if 0
//XXX cbass: check;
PUBLIC static inline
Mword
Mem_space::xlate_flush(unsigned char rights)
{
  Mword a = Page_references;
  if (rights & L4_fpage::RX)
    a |= Page_all_attribs;
  else if (rights & L4_fpage::W)
    a |= Page_writable;

  return a;
}
#endif

//XXX cbass: check;
PUBLIC static inline
L4_fpage::Rights
Mem_space::is_full_flush(L4_fpage::Rights rights)
{
  return rights & L4_fpage::Rights::R(); // CHECK!
}

#if 0
PUBLIC static inline
unsigned char
Mem_space::xlate_flush_result(Mword attribs)
{
  unsigned char r = 0;
  if (attribs & Page_referenced)
    r |= L4_fpage::RX;

  if (attribs & Page_dirty)
    r |= L4_fpage::W;

  return r;
}
#endif

PUBLIC inline NEEDS["cpu.h"]
static bool
Mem_space::has_superpages()
{
  return Cpu::have_superpages();
}

//we flush tlb in htab implementation
IMPLEMENT static inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush(bool = false)
{
  //Mem_unit::tlb_flush();
}


PUBLIC inline
bool
Mem_space::set_attributes(Virt_addr virt, Attr page_attribs)
{
/*
  Pdir::Iter i = _dir->walk(virt);

  if (!i.e->valid() || i.shift() != Config::PAGE_SHIFT)
    return 0;

  i.e->del_attr(Page::MAX_ATTRIBS);
  i.e->add_attr(page_attribs);
  return true;
*/
  NOT_IMPL_PANIC;
  (void)virt; (void)page_attribs;
  return false;
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

  // free ldt memory if it was allocated
  //free_ldt_memory();

  // free all page tables we have allocated for this address space
  // except the ones in kernel space which are always shared
  /*
  _dir->alloc_cast<Mem_space_q_alloc>()
    ->destroy(0, Kmem::mem_user_max, 0, Pdir::Depth,
              Mem_space_q_alloc(_quota, Kmem_alloc::allocator()));
*/
  NOT_IMPL_PANIC;
}

IMPLEMENT inline
Mem_space *
Mem_space::current_mem_space(Cpu_number cpu) /// XXX: do not fix, deprecated, remove!
{
  return _current.cpu(cpu);
}

/** Insert a page-table entry, or upgrade an existing entry with new
    attributes.
    @param phys Physical address (page-aligned).
    @param virt Virtual address for which an entry should be created.
    @param size Size of the page frame -- 4KB or 4MB.
    @param page_attribs Attributes for the mapping (see
                        Mem_space::Page_attrib).
    @return Insert_ok if a new mapping was created;
             Insert_warn_exists if the mapping already exists;
             Insert_warn_attrib_upgrade if the mapping already existed but
                                        attributes could be upgraded;
             Insert_err_nomem if the mapping could not be inserted because
                              the kernel is out of memory;
             Insert_err_exists if the mapping could not be inserted because
                               another mapping occupies the virtual-address
                               range
    @pre phys and virt need to be size-aligned according to the size argument.
 */
IMPLEMENT inline
Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order size,
		    Attr page_attribs)
{
  //assert(size == Size(Config::PAGE_SIZE)
  //       || size == Size(Config::SUPERPAGE_SIZE));
/*
  printf("v_insert: phys %08lx virt %08lx (%s) %p\n", phys, virt,
         page_attribs & Page_writable?"rw":"ro", this);*/
#ifdef FIX_THIS
  Pte_ptr e(phys.value());
  unsigned attribs = e.to_htab_entry(page_attribs);

  Status status = v_insert_cache(&e, cxx::int_value<Vaddr>(virt),
                                 size, attribs);
  return status;
#else
  (void)phys; (void)virt; (void)size; (void)page_attribs;
  return Insert_err_nomem;
#endif
}

IMPLEMENT
bool
Mem_space::try_htab_fault(Address virt)
{
  //bool super;
  //Evict evict;
  //Address pte_ptr, phys;
  Dir_type *dir = _dir;

  if(virt > Mem_layout::User_max)
    dir = Kmem::kdir();

#ifdef FIX_THIS
  Pdir::Iter i = dir->walk(Addr(virt), Pdir::Super_level);

  if(!i.e->valid())
    return false;

  super = i.e->is_super_page();

  i = dir->walk(Addr(virt));

  if(!i.e->is_htab_entry() && !super)
    return false;

  if(super && !i.e->valid())
    {
      i = dir->walk(Addr(virt & Config::SUPERPAGE_MASK));
      phys = Pte_htab::pte_to_addr(i.e);
      phys += (virt & Config::PAGE_MASK) - (phys & Config::PAGE_MASK);
    }
  else
      phys = i.e->raw();

  Status status;
  // insert in htab
  {
     auto guard = lock_guard(cpu_lock);

     status = v_insert_htab(phys, virt, &pte_ptr, &evict);

     // something had to be replaced update  in cache-page table
     if(EXPECT_FALSE(status == Insert_err_nomem)) 
      {
        Pte_base e(evict.phys);
        e.to_htab_entry();
        //printf("EVICTING: virt: %lx phys: %lx\n", evict.virt, e.raw());
        status = v_insert_cache(&e, evict.virt, Config::PAGE_SIZE, 0, evict.dir);
      }
  }


  if(EXPECT_FALSE(status != Insert_ok))
    return false;

  // set pointer in cache
  Pte_base e(pte_ptr);
  e.to_htab_ptr();
  assert(e.addr());

  status  = v_insert_cache(&e, virt, Config::PAGE_SIZE, 0, dir);

  if(EXPECT_FALSE(status != Insert_ok))
#endif
    return false;

  return true;
}

/**
 * Simple page-table lookup.
 *
 * @param virt Virtual address.  This address does not need to be page-aligned.
 * @return Physical address corresponding to a.
 */
PUBLIC inline NEEDS ["paging.h"]
Address
Mem_space::virt_to_phys (Address virt) const
{
  return dir()->virt_to_phys(virt);
}

PUBLIC inline
virtual Address
Mem_space::virt_to_phys_s0(void *a) const
{
  return dir()->virt_to_phys((Address)a);
}

PUBLIC inline
Address
Mem_space::pmem_to_phys (Address virt) const
{
  return virt;
}



/** Look up a page-table entry.
    @param virt Virtual address for which we try the look up.
    @param phys Meaningful only if we find something (and return true).
                If not 0, we fill in the physical address of the found page
                frame.
    @param page_attribs Meaningful only if we find something (and return true).
                If not 0, we fill in the page attributes for the found page
                frame (see Mem_space::Page_attrib).
    @param size If not 0, we fill in the size of the page-table slot.  If an
                entry was found (and we return true), this is the size
                of the page frame.  If no entry was found (and we
                return false), this is the size of the free slot.  In
                either case, it is either 4KB or 4MB.
    @return True if an entry was found, false otherwise.
 */
IMPLEMENT
bool
Mem_space::v_lookup(Vaddr virt, Phys_addr *phys, Page_order *order,
                    Attr *page_attribs)
{
  (void)virt; (void)phys; (void)order; (void)page_attribs;
#ifdef FIX_THIS
  Pdir::Iter i = _dir->walk(virt, Pdir::Super_level);

  if (size)
    *size = Size(1UL << i.shift());

  if(!i.e->valid())
    return false;

  unsigned shift = i.e->is_super_page() ? Config::SUPERPAGE_SHIFT
                   : Config::PAGE_SHIFT;
  unsigned mask = (~0UL << shift);

  i = _dir->walk(virt & Addr(mask));

  if(!i.e->valid())
    return false;

  if (size) *size = Size(1UL << i.shift());

  if (phys || page_attribs)
    {
      Address addr = phys->value();
      Pte_htab::pte_lookup(i.e, &addr, page_attribs);
      *phys = Phys_addr(addr);
    }

  if (page_attribs)
    *page_attribs = to_kernel_fmt(*page_attribs, i.e->is_htab_entry());

  return true;
#else
  return false;
#endif
}

/** Delete page-table entries, or some of the entries' attributes.  This
    function works for one or multiple mappings (in contrast to v_insert!).
    @param virt Virtual address of the memory region that should be changed.
    @param size Size of the memory region that should be changed.
    @param page_attribs If nonzero, delete only the given page attributes.
                        Otherwise, delete the whole entries.
    @return Combined (bit-ORed) page attributes that were removed.  In
            case of errors, ~Page_all_attribs is additionally bit-ORed in.
 */
IMPLEMENT
L4_fpage::Rights
Mem_space::v_delete(Vaddr virt, Page_order size,
		    L4_fpage::Rights page_attribs)
{
#ifdef FIX_THIS
  unsigned ret = 0;
  // delete pages from page tables
  //printf("v_delete: %lx dir: %p\n", virt, _dir);
  assert (size == Size(Config::PAGE_SIZE) 
          || size == Size(Config::SUPERPAGE_SIZE));

  unsigned shift = (size == Virt_size(Config::SUPERPAGE_SIZE)) ?
                    Config::SUPERPAGE_SHIFT : Config::PAGE_SHIFT;

  Address offs = Virt_addr(virt).value() & (~0UL << shift);
  Pdir::Iter i = _dir->walk(Addr(offs));
  Pt_entry *e = nonull_static_cast<Pt_entry*>(i.e);
  for(offs = 0;
      offs < ((Virt_size(size).value() / Config::PAGE_SIZE) *sizeof(Mword));
      offs += sizeof(Mword))
    {
      e = reinterpret_cast<Pt_entry*>(e + offs);

      if(!e->valid())
        continue;

      //in htab ?
      if(!e->is_htab_entry())
        {
          ret = v_delete_htab(e->raw(), page_attribs);

          if(page_attribs & Page_user_accessible)
            v_delete_cache(e, page_attribs);
        }
      else
        ret = v_delete_cache(e, page_attribs);
    }

  if(size != Virt_size(Config::SUPERPAGE_SIZE) && !(page_attribs & Page_user_accessible))
    return ret;

  //check for and delete super page
  i = _dir->walk(virt, Pdir::Super_level);
  i.e = 0;

  return ret;
#else
  (void)virt; (void)size; (void)page_attribs;
  return page_attribs;
#endif
}

PUBLIC static inline
Page_number
Mem_space::canonize(Page_number v)
{ return v; }

IMPLEMENT inline
void
Mem_space::v_set_access_flags(Vaddr, L4_fpage::Rights)
{}

PUBLIC static inline NEEDS["mem_unit.h"]
void
Mem_space::tlb_flush_spaces(bool all, Mem_space *s1, Mem_space *s2)
{
  (void)all; (void)s1; (void)s2;
}
