INTERFACE:

#include <auto_quota.h>
#include <cxx/slist>

#include "config.h"
#include "spin_lock.h"
#include "lock_guard.h"
#include "initcalls.h"

class Buddy_alloc;
class Mem_region_map_base;
class Kip;
class Mem_desc;

template<typename Q> class Kmem_q_alloc;

class Kmem_alloc
{
  friend class Kmem_alloc_tester;

  Kmem_alloc() FIASCO_INIT;

public:
  typedef Buddy_alloc Alloc;

  Address to_phys(void *v) const;

private:
  /**
   * Validate free memory region.
   *
   * The default implementation will always return true. A platform can
   * override the method to constrain the allowed heap region.
   */
  static bool validate_free_region(Kip const *kip, unsigned long *start,
                                   unsigned long *end);

  typedef Spin_lock<> Lock;

  /**
   * Start address fixup of a memory descriptor.
   *
   * We use this for permanent memory allocation directly from the memory
   * descriptors. The start address of a memory descriptor needs to be fixed up
   * temporarily for the purpose of the kernel memory initialization.
   *
   * Since the memory descriptor cannot be altered directly, this structure
   * stores an updated value of the start address of a memory descriptor from
   * which some memory has been allocated.
   */
  struct Fixup
  {
    Mem_desc const *md;  /**< Memory descriptor to fixup. */
    unsigned long start; /**< Fixup start address. */
  };

  /**
   * Number of memory descriptor fixups.
   *
   * Most architectures need just one fixup. On x86, since we allocate the
   * freemap and the TSSs, we might potentially need two fixups.
   */
  enum : size_t { Nr_fixups = 2 };

  static Fixup _fixups[Nr_fixups];
  static Lock lock;
  static Alloc *a;
  static unsigned long _orig_free;
  static Kmem_alloc *_alloc;

};

class Kmem_alloc_reaper : public cxx::S_list_item
{
  size_t (*_reap)(bool desperate);

private:
  typedef cxx::S_list_bss<Kmem_alloc_reaper> Reaper_list;
  static Reaper_list mem_reapers;
};

template<typename Q>
class Kmem_q_alloc
{
public:
  Kmem_q_alloc(Q *q, Kmem_alloc *a) : _a(a), _q(q) {}
  bool valid() const { return _a && _q; }
  void *alloc(Bytes size) const
  {
    Auto_quota<Q> q(_q, size);
    if (EXPECT_FALSE(!q))
      return 0;

    void *b;
    if (EXPECT_FALSE(!(b = _a->alloc(size))))
      return 0;

    q.release();
    return b;
  }

  void free(void *block, Bytes size) const
  {
    _a->free(size, block);
    _q->free(size);
  }

  template<typename V>
  Phys_mem_addr::Value to_phys(V v) const
  { return _a->to_phys(v); }

private:
  Kmem_alloc *_a;
  Q *_q;
};

IMPLEMENTATION:

#include <cassert>

#include "atomic.h"
#include "kip.h"
#include "koptions.h"
#include "mem_layout.h"
#include "mem_region.h"
#include "buddy_alloc.h"
#include "panic.h"
#include "warn.h"

static Kmem_alloc::Alloc _a;

Kmem_alloc::Fixup Kmem_alloc::_fixups[Kmem_alloc::Nr_fixups];
Kmem_alloc::Alloc *Kmem_alloc::a = &_a;
unsigned long Kmem_alloc::_orig_free;
Kmem_alloc::Lock Kmem_alloc::lock;
Kmem_alloc *Kmem_alloc::_alloc;

/**
 * Allocate physical memory before the buddy allocator initialization.
 *
 * This method is used to permanently allocate the physical memory for the buddy
 * allocator freemap, for the TSSs on x86 and potentially other permanent kernel
 * structures. Since these structures are never freed, they do not have to
 * influence the run-time allocation.
 *
 * The memory is allocated directly from the memory descriptors which are fixed
 * up appropriately. If the allocation fails, the method panics.
 *
 * \note This method can be called only before the buddy allocator
 *       initialization.
 *
 * \param size       Size of the structure to allocate.
 * \param alignment  Alignment order of the structure to allocate (log2).
 *
 * \return Physical address of the allocated structure.
 */
PRIVATE static FIASCO_INIT
Address
Kmem_alloc::permanent_alloc(size_t size, Order alignment = Order(0))
{
  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (md.type() != Mem_desc::Kernel_tmp)
        continue;

      unsigned long start = fixup_start(md);
      unsigned long span = fixup_size(md);

      if (span < size)
        continue;

      unsigned long start_aligned
        = cxx::ceil_lsb(start, cxx::int_value<Order>(alignment));
      unsigned long gap = start_aligned - start;

      if (gap >= span)
        continue;

      if (gap + size > span)
        continue;

      if (gap + size < span)
        set_fixup_start(md, start + gap + size);
      else
        md.type(Mem_desc::Reserved);

      return start_aligned;
    }

  panic("Unable to allocate %zu bytes of physical memory", size);
}

/**
 * Get memory descriptor start address with a fixup.
 *
 * \param md  Memory descriptor.
 *
 * \return Memory descriptor start address (potentially fixed up).
 */
PRIVATE static FIASCO_INIT
unsigned long
Kmem_alloc::fixup_start(Mem_desc const &md)
{
  assert(md.type() == Mem_desc::Kernel_tmp);

  /* Look for an existing fixup. */
  for (unsigned int i = 0; i < Nr_fixups; ++i)
    if (_fixups[i].md == &md)
      return _fixups[i].start;

  /* No fixup found, return original memory descriptor start. */
  return md.start();
}

/**
 * Set memory descriptor start address fixup.
 *
 * If there is no space to store the memory descriptor fixup, the method
 * panics.
 *
 * \param md     Memory descriptor.
 * \param start  Fixup value of the start address.
 */
PRIVATE static FIASCO_INIT
void
Kmem_alloc::set_fixup_start(Mem_desc const &md, unsigned long start)
{
  assert(md.type() == Mem_desc::Kernel_tmp);

  /* Look for an existing fixup to update. */
  for (unsigned int i = 0; i < Nr_fixups; ++i)
    {
      if (_fixups[i].md == &md)
        {
          _fixups[i].start = start;
          return;
        }

      /*
       * Fixups are never removed. Therefore if we encounter an unused fixup
       * slot, we can be sure that we have already examined all existing
       * fixups. We create a new fixup in that case.
       */
      if (_fixups[i].md == nullptr)
        {
          _fixups[i].md = &md;
          _fixups[i].start = start;
          return;
        }
    }

  panic("Insufficient number of kernel memory fixup slots");
}

/**
 * Get memory descriptor size with a fixup.
 *
 * \param md  Memory descriptor.
 *
 * \return Memory descriptor size (potentially fixed up).
 */
PRIVATE static FIASCO_INIT
unsigned long
Kmem_alloc::fixup_size(Mem_desc const &md)
{
  assert(md.type() == Mem_desc::Kernel_tmp);

  /* Look for an existing fixup. */
  for (unsigned int i = 0; i < Nr_fixups; ++i)
    if (_fixups[i].md == &md)
      return md.end() - _fixups[i].start + 1;

  /* No fixup found, return original memory descriptor size. */
  return md.size();
}

PUBLIC static inline NEEDS[<cassert>]
Kmem_alloc *
Kmem_alloc::allocator()
{
  assert (_alloc /* uninitialized use of Kmem_alloc */);
  return _alloc;
}

PUBLIC template<typename Q> static inline NEEDS[<cassert>]
Kmem_q_alloc<Q>
Kmem_alloc::q_allocator(Q *quota)
{
  assert (_alloc /* uninitialized use of Kmem_alloc */);
  return Kmem_q_alloc<Q>(quota, _alloc);
}

PROTECTED static
void
Kmem_alloc::allocator(Kmem_alloc *a)
{
  _alloc = a;
}

PUBLIC static FIASCO_INIT
void
Kmem_alloc::init()
{
  static Kmem_alloc al;
  Kmem_alloc::allocator(&al);
}

PUBLIC
void
Kmem_alloc::dump() const
{ a->dump(); }

PUBLIC inline //NEEDS [Kmem_alloc::alloc]
void *
Kmem_alloc::alloc(Order o)
{
  return alloc(Bytes(1) << o);
}

PUBLIC inline //NEEDS [Kmem_alloc::free]
void
Kmem_alloc::free(Order o, void *p)
{
  free(Bytes(1) << o, p);
}

PUBLIC template<typename T> inline
T *
Kmem_alloc::alloc_array(unsigned elems)
{
  return new (this->alloc(Bytes(sizeof(T) * elems))) T[elems];
}

PUBLIC template<typename T> inline
void
Kmem_alloc::free_array(T *b, unsigned elems)
{
  for (unsigned i = 0; i < elems; ++i)
    b[i].~T();
  this->free(Bytes(sizeof(T) * elems), b);
}

PUBLIC
void *
Kmem_alloc::alloc(Bytes size)
{
  const size_t sz = cxx::int_value<Bytes>(size);
  assert(sz >= 8 /* NEW INTERFACE PARANOIA */);
  void* ret;

  {
    auto guard = lock_guard(lock);
    ret = a->alloc(sz);
  }

  if (!ret)
    {
      Kmem_alloc_reaper::morecore (/* desperate= */ true);

      auto guard = lock_guard(lock);
      ret = a->alloc(sz);
    }

  if (EXPECT_FALSE(!ret))
    WARNX(Error, "Out of memory requesting 0x%lx bytes!\n",
          cxx::int_value<Bytes>(size));

  return ret;
}

PUBLIC
void
Kmem_alloc::free(Bytes size, void *page)
{
  const size_t sz = cxx::int_value<Bytes>(size);
  assert(sz >= 8 /* NEW INTERFACE PARANOIA */);
  auto guard = lock_guard(lock);
  a->free(page, sz);
}

IMPLEMENT_DEFAULT static inline NEEDS["mem_layout.h"]
Address
Kmem_alloc::to_phys(void *v) const
{
  return Mem_layout::pmem_to_phys(v);
}

IMPLEMENT_DEFAULT static inline
bool
Kmem_alloc::validate_free_region(Kip const *, unsigned long *, unsigned long *)
{ return true; }

/**
 * Create map entries for all regions which could be used for kernel memory.
 *
 * This is actually the difference quantity of the conventional memory and all
 * unusable memory regions.
 *
 * \param kip        The KIP.
 * \param[out] map   The map containing the difference quantity of conventional
 *                   memory and unusable memory regions.
 * \param alignment  The required kernel memory alignment.
 * \returns  The amount of detected conventional memory in bytes. The amount of
 *           actually usable memory is smaller if any unusable region overlaps
 *           conventional memory.
 */
PRIVATE static FIASCO_INIT
unsigned long
Kmem_alloc::create_free_map(Kip const *kip, Mem_region_map_base *map,
                            unsigned long alignment = Config::PAGE_SIZE)
{
  unsigned long available_size = 0;

  for (auto const &md: kip->mem_descs_a())
    {
      if (!md.valid())
        {
          const_cast<Mem_desc &>(md).type(Mem_desc::Undefined);
          continue;
        }

      if (md.is_virtual())
        continue;

      unsigned long s = md.start();
      unsigned long e = md.end();

      // Sweep out stupid descriptors (that have the end before the start)

      switch (md.type())
        {
        case Mem_desc::Conventional:
          s = (s + alignment - 1) & ~(alignment - 1);
          e = ((e + 1) & ~(alignment - 1)) - 1;
          if (e <= s)
            break;
          if (!validate_free_region(kip, &s, &e))
            break;
          available_size += e - s + 1;
          if (!map->add(Mem_region(s, e)))
            panic("Kmem_alloc::create_free_map(): memory map too small");
          break;
        case Mem_desc::Reserved:
        case Mem_desc::Dedicated:
        case Mem_desc::Shared:
        case Mem_desc::Arch:
        case Mem_desc::Bootloader:
          s = s & ~(alignment - 1);
          e = ((e + alignment) & ~(alignment - 1)) - 1;
          if (!map->sub(Mem_region(s, e)))
            panic("Kmem_alloc::create_free_map(): memory map too small");
          break;
        default:
          break;
        }
    }

  return available_size;
}

PRIVATE static FIASCO_INIT
unsigned long
Kmem_alloc::determine_kmem_alloc_size(unsigned long available_size,
                                      unsigned long alignment = Config::PAGE_SIZE)
{
  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < 1 << 18)
    panic("Kmem_alloc: No kernel memory available (%ld)\n", available_size);

  unsigned long alloc_size = Koptions::o()->kmemsize << 10;
  if (!alloc_size)
    alloc_size = Config::kmem_size(available_size);

  alloc_size = (alloc_size + alignment - 1) & ~(alignment - 1);

  printf("Reserved %lu MiB as kernel memory.\n", alloc_size >> 20);
  return alloc_size;
}

PRIVATE static FIASCO_INIT
void
Kmem_alloc::setup_kmem_from_kip_md_tmp(unsigned long freemap_size,
                                       Address min_addr_kern)
{
  if (0)
    printf("Kmem_alloc: buddy freemap needs %lu bytes\n", freemap_size);

  Address freemap_addr = permanent_alloc(freemap_size);
  Address freemap_addr_kern = Mem_layout::phys_to_pmem(freemap_addr);

  // Strictly speaking this is not necessary but it also doesn't make sense to
  // initialize the lower boundary of the kernel memory at the buddy freemap.
  if (min_addr_kern == freemap_addr_kern)
    min_addr_kern += freemap_size;

  if (0)
    printf("Kmem_alloc: allocator base = %014lx\n",
           Kmem_alloc::Alloc::calc_base_addr(min_addr_kern));

  a->init(min_addr_kern);
  a->setup_free_map(reinterpret_cast<unsigned long *>(freemap_addr_kern),
                    freemap_size);

  // Add all KIP memory regions marked as "Kernel_tmp" to kernel memory.
  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (md.type() != Mem_desc::Kernel_tmp)
        continue;

      unsigned long start = fixup_start(md);
      unsigned long size = fixup_size(md);
      Address kern = Mem_layout::phys_to_pmem(start);

      if (0)
        printf("  Kmem_alloc: block %014lx(%014lx) size=%lx\n",
               kern, start, size);

      a->add_mem(reinterpret_cast<void *>(kern), size);
      md.type(Mem_desc::Reserved);
      _orig_free += size;
    }
}

PUBLIC template< typename Q >
inline
void *
Kmem_alloc::q_alloc(Q *quota, Order order)
{
  Auto_quota<Q> q(quota, order);
  if (EXPECT_FALSE(!q))
    return 0;

  void *b = alloc(order);
  if (EXPECT_FALSE(!b))
    return 0;

  q.release();
  return b;
}

PUBLIC template< typename Q >
inline
void *
Kmem_alloc::q_alloc(Q *quota, Bytes size)
{
  Auto_quota<Q> q(quota, size);
  if (EXPECT_FALSE(!q))
    return 0;

  void *b;
  if (EXPECT_FALSE(!(b = alloc(size))))
    return 0;

  q.release();
  return b;
}


PUBLIC inline NEEDS["mem_layout.h"]
void Kmem_alloc::free_phys(Order o, Address p)
{
  Address va = Mem_layout::phys_to_pmem(p);
  if (va != ~0UL)
    free(o, reinterpret_cast<void *>(va));
}

PUBLIC template< typename Q >
inline
void
Kmem_alloc::q_free_phys(Q *quota, Order order, Address obj)
{
  free_phys(order, obj);
  quota->free(Bytes(1) << order);
}

PUBLIC template< typename Q >
inline
void
Kmem_alloc::q_free(Q *quota, Order order, void *obj)
{
  free(order, obj);
  quota->free(Bytes(1) << order);
}

PUBLIC template< typename Q >
inline
void
Kmem_alloc::q_free(Q *quota, Bytes size, void *obj)
{
  free(size, obj);
  quota->free(size);
}

PUBLIC static inline
unsigned long
Kmem_alloc::orig_free()
{
  return _orig_free;
}


Kmem_alloc_reaper::Reaper_list Kmem_alloc_reaper::mem_reapers;

PUBLIC inline NEEDS["atomic.h"]
Kmem_alloc_reaper::Kmem_alloc_reaper(size_t (*reap)(bool desperate))
: _reap(reap)
{
  mem_reapers.add(this, cas<cxx::S_list_item *>);
}

PUBLIC static
size_t
Kmem_alloc_reaper::morecore(bool desperate = false)
{
  size_t freed = 0;

  for (Reaper_list::Const_iterator reaper = mem_reapers.begin();
       reaper != mem_reapers.end(); ++reaper)
    freed += reaper->_reap(desperate);

  return freed;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Kmem_alloc::debug_dump()
{
  a->dump();

  unsigned long free = a->avail();
  printf("Used %llu%%, %luKiB out of %luKiB of Kmem\n",
         100ULL * (_orig_free - free) / _orig_free,
         (_orig_free - free + 1023) / 1024, (_orig_free + 1023) / 1024);
}
