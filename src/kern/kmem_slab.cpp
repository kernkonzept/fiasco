INTERFACE:

#include <cstddef>		// size_t
#include "boot_alloc.h"
#include "buddy_alloc.h"
#include "kmem_alloc.h"
#include "config.h"
#include "lock_guard.h"
#include "spin_lock.h"

#include "slab_cache.h"		// Slab_cache
#include "minmax.h"
#include "per_node_data.h"

#include <cxx/slist>
#include <cxx/type_traits>

class Global_alloc
{
  typedef Spin_lock<> Lock;
  static Per_node_data<Lock> _lock;

protected:
  static void *unaligned_alloc(size_t size)
  {
    auto guard = lock_guard(*_lock);
    return Boot_alloced::alloc(size);
  }

  template<typename Q> static
  void *q_unaligned_alloc(Q *quota, size_t size)
  {
    Auto_quota<Q> q(quota, size);
    if (EXPECT_FALSE(!q))
      return 0;

    void *b;
    if (EXPECT_FALSE(!(b=unaligned_alloc(size))))
      return 0;

    q.release();
    return b;
  }

  static void unaligned_free(size_t size, void *e)
  {
    auto guard = lock_guard(*_lock);
    Boot_alloced::free(size, e);
  }

  template<typename Q> static
  void q_unaligned_free(Q* quota, size_t size, void *e)
  {
    Boot_alloced::free(size, e);
    quota->free(size);
  }
};

class Kmem_slab : public Slab_cache, public cxx::S_list_item
{
  friend class Jdb_kern_info_memory;
  typedef cxx::S_list_bss<Kmem_slab> Reap_list;

  // STATIC DATA
  static Per_node_data<Reap_list> reap_list;
};


/**
 * Slab allocator for the given size and alignment.
 * \tparam SIZE   Size of an object in bytes.
 * \tparam ALIGN  Alignment of an object in bytes. Must be power of 2.
 *
 * This class provides a per-size instance of a slab cache.
 *
 * Note: Kmem_buddy_for_size, Kmem_slab_for_size and Global_for_size must
 * provide compatible interfaces in order to be used interchangeably.
 */
template< unsigned SIZE, unsigned ALIGN = 8 >
class Kmem_slab_for_size
{
  static_assert(
      (ALIGN > 0) && !(ALIGN & (ALIGN - 1)),
      "alignment must be power of 2");
  static_assert(
      Slab_cache::entry_size(SIZE, ALIGN) >= Slab_cache::Min_obj_size,
      "objects too small for slab");

public:
  Kmem_slab_for_size() : _s(SIZE, ALIGN, "fixed size") {}

  void *alloc() { return _s.alloc(); }

  template<typename Q>
  void *q_alloc(Q *q) { return _s.template q_alloc<Q>(q); }

  void free(void *e) { _s.free(e); }

  template<typename Q>
  void q_free(Q *q, void *e) { _s.template q_free<Q>(q, e); }

protected:
  Kmem_slab _s;
};


/**
 * Allocator for fixed-size objects based on the buddy allocator.
 * \tparam SIZE  The size and the alignment of an object in bytes.
 *
 * Note: This allocator uses the buddy allocator and allocates memory in sizes
 * supported by the buddy allocator.
 *
 * Note: Kmem_buddy_for_size, Kmem_slab_for_size and Global_for_size must
 * provide compatible interfaces in order to be used interchangeably.
 */
template<unsigned SIZE>
class Kmem_buddy_for_size
{
public:
  static void *alloc()
  { return Kmem_alloc::allocator()->alloc(Bytes(SIZE)); }

  template<typename Q> static
  void *q_alloc(Q *q)
  { return Kmem_alloc::allocator()->q_alloc(q, Bytes(SIZE)); }

  static void free(void *e)
  { Kmem_alloc::allocator()->free(Bytes(SIZE), e); }

  template<typename Q> static
  void q_free(Q *q, void *e)
  { Kmem_alloc::allocator()->q_free(q, Bytes(SIZE), e); }
};


/**
 * Allocator for fixed-size objects using the global Boot_alloced allocator.
 * \tparam SIZE  The size of an object in bytes.
 *
 * This allocator re-uses the slow best-fit boot time allocator. It should only
 * be used on resource constrained systems where saving memory is paramount
 * compared to the slower allocation speed.
 *
 * Note: Kmem_buddy_for_size, Kmem_slab_for_size and Global_for_size must
 * provide compatible interfaces in order to be used interchangeably.
 */
template<unsigned SIZE>
class Global_for_size : public Global_alloc
{
public:
  static void *alloc()
  { return unaligned_alloc(SIZE); }

  template<typename Q> static
  void *q_alloc(Q *q)
  { return q_unaligned_alloc<Q>(q, SIZE); }

  static void free(void *e)
  { unaligned_free(SIZE, e); }

  template<typename Q> static
  void q_free(Q *q, void *e)
  { q_unaligned_free<Q>(q, SIZE, e); }
};

/**
 * Meta allocator to select between Global_for_size, Kmem_slab_for_size and
 * Kmem_buddy_for_size.
 *
 * \tparam SLABS  If true some sort of slab allocator will be used. Otherwise
 *                Global_for_size is always used.
 * \tparam BUDDY  If true the allocator will use Kmem_buddy_for_size, if
 *                false Kmem_slab_for_size.
 * \tparam SIZE   Size of an object (in bytes) that shall be allocated.
 * \tparam ALIGN  Alignment for each object in bytes. Must be power of 2.
 */
template<bool SLABS, bool BUDDY, unsigned SIZE, unsigned ALIGN>
struct _Kmem_alloc : Kmem_slab_for_size<SIZE, ALIGN> {};

/* Specialization using the global allocator without slab */
template<bool BUDDY, unsigned SIZE, unsigned ALIGN>
struct _Kmem_alloc<false, BUDDY, SIZE, ALIGN> : Global_for_size<SIZE> {};

/* Specialization using the buddy allocator */
template<unsigned SIZE, unsigned ALIGN>
struct _Kmem_alloc<true, true, SIZE, ALIGN> : Kmem_buddy_for_size<SIZE>
{
  static_assert(
      (ALIGN > 0) && !(ALIGN & (ALIGN - 1)),
      "alignment must be power of 2");
  static_assert(
      ALIGN <= SIZE,
      "alignment constraint too strict for buddy allocator");
  static_assert(
      SIZE <= Buddy_alloc::Max_size,
      "size cannot be bigger than maximum buddy allocator block size");
};

/**
 * Generic allocator for objects of the given size and alignment.
 * \tparam SIZE   The size of an object in bytes.
 * \tparam ALIGN  The alignment of each allocated object (in bytes).
 *                Must be power of 2.
 *
 * This allocator uses _Kmem_alloc<> to select between a slab allocator or
 * the buddy allocator depending on the given size of the objects.
 * (Currently all objects bigger that 1KB (0x400) are allocated using the
 * buddy allocator.)
 */
template<unsigned SIZE, unsigned ALIGN = 8>
struct Kmem_slab_s
: _Kmem_alloc<Config::Slab_allocators, (SIZE >= 0x400), SIZE, ALIGN>
{};

/**
 * Allocator for objects of the given type.
 * \tparam T      Type of the object to be allocated.
 * \tparam ALIGN  Alignment of the objects (in bytes, usually alignof(T)).
 *                Must be power of 2.
 */
template< typename T, unsigned ALIGN = __alignof(T) >
struct Kmem_slab_t
: Kmem_slab_s<sizeof(T), max<unsigned>(ALIGN, Slab_cache::Min_obj_align)>
{
  typedef Kmem_slab_s<sizeof(T), max<unsigned>(ALIGN, Slab_cache::Min_obj_align)> Slab;
public:
  explicit Kmem_slab_t(char const *) {}
  Kmem_slab_t() = default;

  template<typename ...ARGS>
  T *new_obj(ARGS &&...args)
  {
    void *c = Slab::alloc();
    if (EXPECT_TRUE(c != 0))
      return new (c) T(cxx::forward<ARGS>(args)...);
    return 0;
  }

  template<typename Q, typename ...ARGS>
  T *q_new(Q *q, ARGS &&...args)
  {
    void *c = Slab::template q_alloc<Q>(q);
    if (EXPECT_TRUE(c != 0))
      return new (c) T(cxx::forward<ARGS>(args)...);
    return 0;
  }

  void del(T *e)
  {
    e->~T();
    Slab::free(e);
  }

  template<typename Q>
  void q_del(Q *q, T *e)
  {
    e->~T();
    Slab::template q_free<Q>(q, e);
  }
};

IMPLEMENTATION:

DECLARE_PER_NODE Per_node_data<Global_alloc::Lock> Global_alloc::_lock;

DECLARE_PER_NODE Per_node_data<Kmem_slab::Reap_list> Kmem_slab::reap_list;

// Kmem_slab -- A type-independent slab cache allocator for Fiasco,
// derived from a generic slab cache allocator (Slab_cache in
// lib/slab.cpp).

// This specialization adds low-level page allocation and locking to
// the slab allocator implemented in our base class (Slab_cache).
//-

#include <cassert>
#include "config.h"
#include "atomic.h"
#include "panic.h"
#include "kmem_alloc.h"

// Specializations providing their own block_alloc()/block_free() can
// also request slab sizes larger than one page.
PROTECTED
Kmem_slab::Kmem_slab(unsigned long slab_size,
				   unsigned elem_size,
				   unsigned alignment,
				   char const *name)
  : Slab_cache(slab_size, elem_size, alignment, name)
{
  reap_list->add(this, mp_cas<cxx::S_list_item*>);
}

// Specializations providing their own block_alloc()/block_free() can
// also request slab sizes larger than one page.
PUBLIC
Kmem_slab::Kmem_slab(unsigned elem_size,
                     unsigned alignment,
                     char const *name,
                     unsigned long min_size = Buddy_alloc::Min_size,
                     unsigned long max_size = Buddy_alloc::Max_size)
  : Slab_cache(elem_size, alignment, name, min_size, max_size)
{
  reap_list->add(this, mp_cas<cxx::S_list_item*>);
}

PUBLIC
Kmem_slab::~Kmem_slab()
{
  destroy();
}


// Callback functions called by our super class, Slab_cache, to
// allocate or free blocks

virtual void *
Kmem_slab::block_alloc(unsigned long size, unsigned long) override
{
  assert (size >= Buddy_alloc::Min_size);
  assert (size <= Buddy_alloc::Max_size);
  assert (!(size & (size - 1)));
  (void)size;
  return Kmem_alloc::allocator()->alloc(Bytes(size));
}

virtual void
Kmem_slab::block_free(void *block, unsigned long size) override
{
  Kmem_alloc::allocator()->free(Bytes(size), block);
}

// 
// Memory reaper
// 
PUBLIC static
size_t
Kmem_slab::reap_all (bool desperate)
{
  size_t freed = 0;

  for (auto *alloc: *reap_list)
    {
      size_t got;
      do
	{
	  got = alloc->reap();
	  freed += got;
	}
      while (desperate && got);
    }

  return freed;
}

static DECLARE_PER_NODE Per_node_data<Kmem_alloc_reaper> kmem_slab_reaper(Kmem_slab::reap_all);
