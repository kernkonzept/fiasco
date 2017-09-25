INTERFACE:

#include <cstddef>		// size_t
#include "buddy_alloc.h"
#include "kmem_alloc.h"
#include "config.h"
#include "lock_guard.h"
#include "spin_lock.h"

#include "slab_cache.h"		// Slab_cache
#include <cxx/slist>
#include <cxx/type_traits>

class Kmem_slab : public Slab_cache, public cxx::S_list_item
{
  friend class Jdb_kern_info_memory;
  typedef cxx::S_list_bss<Kmem_slab> Reap_list;

  // STATIC DATA
  static Reap_list reap_list;
};


/**
 * Slab allocator for the given size and alignment.
 * \tparam SIZE   Size of an object in bytes.
 * \tparam ALIGN  Alignment of an object in bytes.
 *
 * This class provides a per-size instance of a slab cache.
 */
template< unsigned SIZE, unsigned ALIGN = 8 >
class Kmem_slab_for_size
{
public:
  static void *alloc() { return _s.alloc(); }

  template<typename Q> static
  void *q_alloc(Q *q) { return _s.template q_alloc<Q>(q); }

  static void free(void *e) { _s.free(e); }

  template<typename Q> static
  void q_free(Q *q, void *e) { _s.template q_free<Q>(q, e); }

  static Slab_cache *slab() { return &_s; }

protected:
  static Kmem_slab _s;
};

template< unsigned SIZE, unsigned ALIGN >
Kmem_slab Kmem_slab_for_size<SIZE, ALIGN>::_s(SIZE, ALIGN, "fixed size");


/**
 * Allocator for fixed-size objects based on the buddy allocator.
 * \tparam SIZE  The size and the alignment of an object in bytes.
 *
 * Note: This allocator uses the buddy allocator and allocates memory in sizes
 * supported by the buddy allocator.
 *
 * Note: Kmem_buddy_for_size and Kmem_slab_for_size must provide compatible
 * interfaces in order to be used interchangeably.
 */
template<unsigned SIZE>
class Kmem_buddy_for_size
{
public:
  static void *alloc()
  { return Kmem_alloc::allocator()->unaligned_alloc(SIZE); }

  template<typename Q> static
  void *q_alloc(Q *q)
  { return Kmem_alloc::allocator()->q_unaligned_alloc(q, SIZE); }

  static void free(void *e)
  { Kmem_alloc::allocator()->unaligned_free(SIZE, e); }

  template<typename Q> static
  void q_free(Q *q, void *e)
  { Kmem_alloc::allocator()->q_unaligned_free(q, SIZE, e); }
};

/**
 * Meta allocator to select between Kmem_slab_for_size and Kmem_buddy_for_size.
 *
 * \tparam BUDDY  If true the allocator will use Kmem_buddy_for_size, if
 *                false Kmem_slab_for_size.
 * \tparam SIZE   Size of an object (in bytes) that shall be allocated.
 * \tparam ALIGN  Alignment for each object in bytes.
 */
template<bool BUDDY, unsigned SIZE, unsigned ALIGN>
struct _Kmem_alloc : Kmem_slab_for_size<SIZE, ALIGN> {};

/* Specialization using the buddy allocator */
template<unsigned SIZE, unsigned ALIGN>
struct _Kmem_alloc<true, SIZE, ALIGN> : Kmem_buddy_for_size<SIZE> {};

/**
 * Generic allocator for objects of the given size and alignment.
 * \tparam SIZE   The size of an object in bytes.
 * \tparam ALIGN  The alignment of each allocated object (in bytes).
 *
 * This allocator uses _Kmem_alloc<> to select between a slab allocator or
 * the buddy allocator depending on the given size of the objects.
 * (Currently all objects bigger that 1KB (0x400) are allocated using the
 * buddy allocator.)
 */
template<unsigned SIZE, unsigned ALIGN = 8>
struct Kmem_slab_s : _Kmem_alloc<(SIZE >= 0x400), SIZE, ALIGN> {};

/**
 * Allocator for objects of the given type.
 * \tparam T      Type of the object to be allocated.
 * \tparam ALIGN  Alignment of the objects (in bytes, usually alignof(T)).
 */
template< typename T, unsigned ALIGN = __alignof(T) >
struct Kmem_slab_t : Kmem_slab_s<sizeof(T), ALIGN>
{
  typedef Kmem_slab_s<sizeof(T), ALIGN> Slab;
public:
  explicit Kmem_slab_t(char const *) {}
  Kmem_slab_t() = default;

  template<typename ...ARGS> static
  T *new_obj(ARGS &&...args)
  {
    void *c = Slab::alloc();
    if (EXPECT_TRUE(c != 0))
      return new (c) T(cxx::forward<ARGS>(args)...);
    return 0;
  }

  template<typename Q, typename ...ARGS> static
  T *q_new(Q *q, ARGS &&...args)
  {
    void *c = Slab::template q_alloc<Q>(q);
    if (EXPECT_TRUE(c != 0))
      return new (c) T(cxx::forward<ARGS>(args)...);
    return 0;
  }

  static void del(T *e)
  {
    e->~T();
    Slab::free(e);
  }

  template<typename Q> static
  void q_del(Q *q, T *e)
  {
    e->~T();
    Slab::template q_free<Q>(q, e);
  }
};

IMPLEMENTATION:

Kmem_slab::Reap_list Kmem_slab::reap_list;

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
  reap_list.add(this, mp_cas<cxx::S_list_item*>);
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
  reap_list.add(this, mp_cas<cxx::S_list_item*>);
}

PUBLIC
Kmem_slab::~Kmem_slab()
{
  destroy();
}


// Callback functions called by our super class, Slab_cache, to
// allocate or free blocks

virtual void *
Kmem_slab::block_alloc(unsigned long size, unsigned long)
{
  assert (size >= Buddy_alloc::Min_size);
  assert (size <= Buddy_alloc::Max_size);
  assert (!(size & (size - 1)));
  (void)size;
  return Kmem_alloc::allocator()->unaligned_alloc(size);
}

virtual void
Kmem_slab::block_free(void *block, unsigned long size)
{
  Kmem_alloc::allocator()->unaligned_free(size, block);
}

// 
// Memory reaper
// 
PUBLIC static
size_t
Kmem_slab::reap_all (bool desperate)
{
  size_t freed = 0;

  for (Reap_list::Const_iterator alloc = reap_list.begin();
       alloc != reap_list.end(); ++alloc)
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

static Kmem_alloc_reaper kmem_slab_reaper(Kmem_slab::reap_all);
