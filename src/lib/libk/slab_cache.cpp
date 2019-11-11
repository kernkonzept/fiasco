INTERFACE:

#include <spin_lock.h>
#include <cxx/hlist>
#include <cxx/slist>
#include <auto_quota.h>

// The anonymous slab allocator.  You can specialize this allocator by
// providing your own initialization functions and your own low-level
// allocation functions.

class Slab : public cxx::H_list_item
{
private:
  Slab(const Slab&);	// copy constructors remain undefined

  typedef cxx::S_list_item Slab_entry;
  typedef cxx::S_list<Slab_entry> Free_list;

  Free_list _free;
  unsigned short _elems;
  unsigned short _in_use;
};

class Slab_cache
{
protected:
  friend class Slab;

  // Low-level allocator functions:

  // Allocate/free a block.  "size" is always a multiple of PAGE_SIZE.
  virtual void *block_alloc(unsigned long size, unsigned long alignment) = 0;
  virtual void block_free(void *block, unsigned long size) = 0;

private:
  Slab_cache();
  Slab_cache(const Slab_cache&); // default constructor is undefined

  //
  // data declaration follows
  // 

  typedef cxx::H_list<Slab> Slab_list;
  Slab_list _full;    ///< List of full slabs
  Slab_list _partial; ///< List of partially filled slabs
  Slab_list _empty;   ///< List of empty slabs


  unsigned long _slab_size;
  unsigned _entry_size, _elem_num;
  unsigned _num_empty;
  typedef Spin_lock<> Lock;
  Lock lock;
  char const *_name;
};


IMPLEMENTATION:

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <lock_guard.h>

// default deallocator must not be called -- must use explicit destruction
inline NOEXPORT
void 
Slab::operator delete(void* /*block*/)
{
  assert (!"slab::operator delete called");
}

PUBLIC
Slab::Slab(unsigned elems, unsigned entry_size, void *mem)
: _elems(elems), _in_use(0)
{
  // Compute pointer to first data element, now taking into account
  // the latest colorization offset
  char *data = reinterpret_cast<char*>(mem);

  // Initialize the cache elements
  for (unsigned i = elems; i > 0; --i)
    {
      Slab_entry *e = reinterpret_cast<Slab_entry *>(data);
      _free.push_front(e);
      data += entry_size;
    }
}

PUBLIC
void *
Slab::alloc()
{
  Slab_entry *e = _free.pop_front();

  if (! e)
    return 0;

  ++_in_use;
  return e;
}

PUBLIC
void
Slab::free(void *entry)
{
  _free.add(reinterpret_cast<Slab_entry *>(entry));

  assert(_in_use);
  --_in_use;
}

PUBLIC
inline bool
Slab::is_empty() const
{
  return _in_use == 0;
}

PUBLIC
inline bool
Slab::is_full() const
{
  return _in_use == _elems;
}

PUBLIC
inline unsigned
Slab::in_use() const
{
  return _in_use;
}

PUBLIC
inline void *
Slab::operator new(size_t, void *block) throw()
{
  // slabs must be size-aligned so that we can compute their addresses
  // from element addresses
  return block;
}


PUBLIC static inline
unsigned
Slab_cache::entry_size(unsigned elem_size, unsigned alignment)
{ return (elem_size + alignment - 1) & ~(alignment - 1); }

// 
// Slab_cache
// 
PUBLIC inline NEEDS[Slab_cache::entry_size]
Slab_cache::Slab_cache(unsigned elem_size, 
				 unsigned alignment,
				 char const * name, 
				 unsigned long min_size,
				 unsigned long max_size)
  : _entry_size(entry_size(elem_size, alignment)), _num_empty(0),
    _name (name)
{
  lock.init();

  for (
      _slab_size = min_size;
      (_slab_size - sizeof(Slab)) / _entry_size < 8
        && _slab_size < max_size;
      _slab_size <<= 1) ;

  _elem_num = (_slab_size - sizeof(Slab)) / _entry_size;
}

//
// Slab_cache
//
PUBLIC inline
Slab_cache::Slab_cache(unsigned long slab_size,
				 unsigned elem_size,
				 unsigned alignment,
				 char const * name)
  : _slab_size(slab_size), _entry_size(entry_size(elem_size, alignment)),
    _num_empty(0), _name (name)
{
  lock.init();
  _elem_num = (_slab_size - sizeof(Slab)) / _entry_size;
}

PUBLIC
virtual
Slab_cache::~Slab_cache()
{
  // the derived class should call destroy() before deleting us.
  // assert(_first_slab == 0);
}

PROTECTED inline
void
Slab_cache::destroy()	// descendant should call this in destructor
{
}

PRIVATE inline NOEXPORT
Slab *
Slab_cache::get_available_locked()
{
  Slab *s = _partial.front();
  if (s)
    return s;

  s = _empty.front();
  if (s)
    {
      --_num_empty;
      _empty.remove(s);
      _partial.add(s);
    }

  return s;
}

PUBLIC
void *
Slab_cache::alloc()	// request initialized member from cache
{
  void *unused_block = 0;
  void *ret;
    {
      auto guard = lock_guard(lock);

      Slab *s = get_available_locked();

      if (EXPECT_FALSE(!s))
	{
	  guard.reset();

	  char *m = (char*)block_alloc(_slab_size, _slab_size);
	  Slab *new_slab = 0;
	  if (m)
	    new_slab = new (m + _slab_size - sizeof(Slab)) Slab(_elem_num, _entry_size, m);

	  guard.lock(&lock);

	  // retry gettin a slab that might be allocated by a different
	  // CPU meanwhile
	  s = get_available_locked();

	  if (!s)
	    {
	      // real OOM
	      if (!m)
		return 0;

	      _partial.add(new_slab);
	      s = new_slab;
	    }
	  else
	    unused_block = m;
	}

      ret = s->alloc();
      assert(ret);

      if (s->is_full())
	{
	  cxx::H_list<Slab>::remove(s);
	  _full.add(s);
	}
    }

  if (unused_block)
    block_free(unused_block, _slab_size);

  return ret;
}

PUBLIC template< typename Q >
inline
void *
Slab_cache::q_alloc(Q *quota)
{
  Auto_quota<Q> q(quota, _entry_size);
  if (EXPECT_FALSE(!q))
    return 0;

  void *r;
  if (EXPECT_FALSE(!(r=alloc())))
    return 0;

  q.release();
  return r;
}

PUBLIC
void
Slab_cache::free(void *cache_entry) // return initialized member to cache
{
  Slab *to_free = 0;
    {
      auto guard = lock_guard(lock);

      Slab *s = reinterpret_cast<Slab*>
	((reinterpret_cast<unsigned long>(cache_entry) & ~(_slab_size - 1)) + _slab_size - sizeof(Slab));

      bool was_full = s->is_full();

      s->free(cache_entry);

      if (was_full)
	{
	  cxx::H_list<Slab>::remove(s);
	  _partial.add(s);
	}
      else if (s->is_empty())
	{
	  cxx::H_list<Slab>::remove(s);
	  if (_num_empty < 2)
	    {
	      _empty.add(s);
	      ++_num_empty;
	    }
	  else
	    to_free = s;
	}
      else
	{
	  // We weren't either full or empty; we already had free
	  // elements.  This changes nothing in the queue, and there
	  // already must have been a _first_available_slab.
	}
    }

  if (to_free)
    {
      to_free->~Slab();
      block_free(reinterpret_cast<char *>(to_free + 1) - _slab_size, _slab_size);
    }
}

PUBLIC template< typename Q >
inline
void
Slab_cache::q_free(Q *quota, void *obj)
{
  free(obj);
  quota->free(_entry_size);
}

PUBLIC
unsigned long
Slab_cache::reap()		// request that cache returns memory to system
{
  Slab *s = 0;
  unsigned long sz = 0;

  for (;;)
    {
	{
	  auto guard = lock_guard(lock);

	  s = _empty.front();
	  // nothing to free
	  if (!s)
	    return 0;

	  cxx::H_list<Slab>::remove(s);
	}

      // explicitly call destructor to delete s;
      s->~Slab();
      block_free(reinterpret_cast<char *>(s + 1) - _slab_size, _slab_size);
      sz += _slab_size;
    }

  return sz;
}

// Debugging output

#include <cstdio>


PUBLIC
void
Slab_cache::debug_dump()
{
  printf ("%s: %lu-KB slabs (elems per slab=%u ",
	  _name, _slab_size / 1024, _elem_num);

  unsigned count = 0, total = 0, total_elems = 0;
  for (Slab_list::Const_iterator s = _full.begin(); s != _full.end(); ++s)
    {
      if (!s->is_full())
	printf ("\n*** wrongly-enqueued full slab found\n");

      ++count;
      total_elems += s->in_use();
    }

  total += count;

  printf ("%u full, ", count);

  count = 0;
  for (Slab_list::Const_iterator s = _partial.begin(); s != _partial.end(); ++s)
    {
      if (s->is_full() || s->is_empty())
	printf ("\n*** wrongly-enqueued full slab found\n");

      count++;
      total_elems += s->in_use();
    }

  total += count;

  printf ("%u used, ", count);

  count = 0;
  for (Slab_list::Const_iterator s = _empty.begin(); s != _empty.end(); ++s)
    {
      if (! s->is_empty())
	printf ("\n*** wrongly-enqueued nonempty slab found\n");
      count++;
    }

  unsigned total_used = total;
  total += count;

  printf ("%u empty = %u total) = %lu KB,\n  %u elems (size=%u)",
	  count, total, total * _slab_size / 1024,
	  total_elems, _entry_size);

  if (total_elems)
    printf (", overhead = %lu B (%lu B)  = %lu%% (%lu%%) \n",
	    total * _slab_size - total_elems * _entry_size,
	    total_used * _slab_size - total_elems * _entry_size,
	    100 - total_elems * _entry_size * 100 / (total * _slab_size),
	    100 - total_elems * _entry_size * 100 / (total_used * _slab_size));
  else
    printf ("\n");
}
