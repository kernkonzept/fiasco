INTERFACE:

#include <auto_quota.h>
#include <cxx/slist>

#include "spin_lock.h"
#include "lock_guard.h"
#include "initcalls.h"

class Buddy_alloc;
class Mem_region_map_base;
class Kip;

template<typename Q> class Kmem_q_alloc;

class Kmem_alloc
{
  Kmem_alloc();

public:
  typedef Buddy_alloc Alloc;
private:
  typedef Spin_lock<> Lock;
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
  void *alloc(unsigned long size) const
  {
    Auto_quota<Q> q(_q, size);
    if (EXPECT_FALSE(!q))
      return 0;

    void *b;
    if (EXPECT_FALSE(!(b=_a->unaligned_alloc(size))))
      return 0;

    q.release();
    return b;
  }

  void free(void *block, unsigned long size) const
  {
    _a->unaligned_free(size, block);
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

#include "config.h"
#include "kip.h"
#include "mem_layout.h"
#include "mem_region.h"
#include "buddy_alloc.h"
#include "panic.h"

static Kmem_alloc::Alloc _a;
Kmem_alloc::Alloc *Kmem_alloc::a = &_a;
unsigned long Kmem_alloc::_orig_free;
Kmem_alloc::Lock Kmem_alloc::lock;
Kmem_alloc* Kmem_alloc::_alloc;

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
  _alloc=a;
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

PUBLIC inline NEEDS [Kmem_alloc::unaligned_alloc]
void *
Kmem_alloc::alloc(size_t o)
{
  return unaligned_alloc(1UL << o);
}


PUBLIC inline NEEDS [Kmem_alloc::unaligned_free]
void
Kmem_alloc::free(size_t o, void *p)
{
  unaligned_free(1UL << o, p);
}

PUBLIC template<typename T> inline
T *
Kmem_alloc::alloc_array(unsigned elems)
{
  return new (this->unaligned_alloc(sizeof(T) * elems)) T[elems];
}

PUBLIC template<typename T> inline
void
Kmem_alloc::free_array(T *b, unsigned elems)
{
  for (unsigned i = 0; i < elems; ++i)
    b[i].~T();
  this->unaligned_free(b, sizeof(T) * elems);
}

PUBLIC 
void *
Kmem_alloc::unaligned_alloc(unsigned long size)
{
  assert(size >=8 /*NEW INTERFACE PARANIOIA*/);
  void* ret;

  {
    auto guard = lock_guard(lock);
    ret = a->alloc(size);
  }

  if (!ret)
    {
      Kmem_alloc_reaper::morecore (/* desperate= */ true);

      auto guard = lock_guard(lock);
      ret = a->alloc(size);
    }

  return ret;
}

PUBLIC
void
Kmem_alloc::unaligned_free(unsigned long size, void *page)
{
  assert(size >=8 /*NEW INTERFACE PARANIOIA*/);
  auto guard = lock_guard(lock);
  a->free(page, size);
}


PRIVATE static FIASCO_INIT
unsigned long
Kmem_alloc::create_free_map(Kip const *kip, Mem_region_map_base *map)
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
	  s = (s + Config::PAGE_SIZE - 1) & ~(Config::PAGE_SIZE - 1);
	  e = ((e + 1) & ~(Config::PAGE_SIZE - 1)) - 1;
	  if (e <= s)
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
	  s = s & ~(Config::PAGE_SIZE - 1);
	  e = ((e + Config::PAGE_SIZE) & ~(Config::PAGE_SIZE - 1)) - 1;
	  if (!map->sub(Mem_region(s, e)))
	    panic("Kmem_alloc::create_free_map(): memory map too small");
	  break;
	default:
	  break;
	}
    }

  return available_size;
}


PUBLIC template< typename Q >
inline
void *
Kmem_alloc::q_alloc(Q *quota, size_t order)
{
  Auto_quota<Q> q(quota, 1UL<<order);
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
Kmem_alloc::q_unaligned_alloc(Q *quota, size_t size)
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


PUBLIC inline NEEDS["mem_layout.h"]
void Kmem_alloc::free_phys(size_t s, Address p)
{
  void *va = (void*)Mem_layout::phys_to_pmem(p);
  if((unsigned long)va != ~0UL)
    free(s, va);
}

PUBLIC template< typename Q >
inline
void 
Kmem_alloc::q_free_phys(Q *quota, size_t order, Address obj)
{
  free_phys(order, obj);
  quota->free(1UL<<order);
}

PUBLIC template< typename Q >
inline
void 
Kmem_alloc::q_free(Q *quota, size_t order, void *obj)
{
  free(order, obj);
  quota->free(1UL<<order);
}

PUBLIC template< typename Q >
inline
void 
Kmem_alloc::q_unaligned_free(Q *quota, size_t size, void *obj)
{
  unaligned_free(size, obj);
  quota->free(size);
}



#include "atomic.h"

Kmem_alloc_reaper::Reaper_list Kmem_alloc_reaper::mem_reapers;

PUBLIC inline NEEDS["atomic.h"]
Kmem_alloc_reaper::Kmem_alloc_reaper(size_t (*reap)(bool desperate))
  : _reap(reap)
{
  mem_reapers.add(this, mp_cas<cxx::S_list_item*>);
}

PUBLIC static
size_t
Kmem_alloc_reaper::morecore(bool desperate = false)
{
  size_t freed = 0;

  for (Reaper_list::Const_iterator reaper = mem_reapers.begin();
       reaper != mem_reapers.end(); ++reaper)
    {
      freed += reaper->_reap(desperate);
    }

  return freed;
}
