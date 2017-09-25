INTERFACE:

#include <spin_lock.h>

// The anonymous slab allocator.  You can specialize this allocator by
// providing your own initialization functions and your own low-level
// allocation functions.

class slab;

class slab_cache_anon
{
protected:
  friend class slab;

  // Low-level allocator functions:

  // Allocate/free a block.  "size" is always a multiple of PAGE_SIZE.
  virtual void *block_alloc(unsigned long size, unsigned long alignment) = 0;
  virtual void block_free(void *block, unsigned long size) = 0;

private:
  typedef Spin_lock Lock;

  Lock lock;
  slab_cache_anon();
  slab_cache_anon(const slab_cache_anon&); // default constructor is undefined

  //
  // data declaration follows
  // 

  // The slabs of this cache are held in a partially-sorted
  // doubly-linked list.  First come the fully-active slabs (all
  // elements in use), then the partially active slabs, then empty
  // slabs.
  slab *_first_slab, *_first_available_slab, *_last_slab;
  unsigned long _slab_size;
  unsigned _elem_size, _latest_offset, _alignment;
  char const *_name;
};				// end declaration of class slab_cache

IMPLEMENTATION:

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <lock_guard.h>

#ifndef offsetof                // should be defined in stddef.h, but isn't
#define offsetof(TYPE, MEMBER) (((size_t) &((TYPE *)10)->MEMBER) - 10)
#endif

// 
// class slab
// 

class slab			// a slab of the cache
{
  slab();
  slab(const slab&);	// default constructors remain undefined

  struct slab_entry
  {
    slab_entry *_next_free;
    char _entry[0];
  };
  
  struct slab_data
  {
    slab_cache_anon *_cache;
    slab_entry *_first_free;
    slab *_next, *_prev;
    unsigned short _in_use, _free;
  };
  
  // slabs for CACHE_ENTRY should contain at least min_cache_items
  // cache entries
  static const unsigned min_cache_items = 4;
  
  // 
  // data declaration follows 
  // 
  slab_data _data;
};				// end declaration of class slab

// 
// class slab
//- 

// default deallocator must not be called -- must use explicit destruction
inline NOEXPORT
void 
slab::operator delete(void* /*block*/)
{
  assert (!"slab::operator delete called");
}

  
PUBLIC
slab::slab(slab_cache_anon *cache)
{
  _data._cache = cache;
  _data._in_use = 0;
  _data._next = _data._prev = 0;

  // Compute offset of first slab_entry in slab, not taking into
  // account the colorization offset.  "slab_entry._entry[]" needs to
  // be "cache->_alignment"-aligned
  unsigned long offset_first_elem = 
    ((sizeof(slab_data) + sizeof (slab_entry) + cache->_alignment - 1) 
     & ~(cache->_alignment - 1)) 
    - sizeof (slab_entry);

  // Compute size of a slab entry, including alignment padding at end
  // of entry
  unsigned entry_size = 
    (sizeof(slab_entry) + cache->_elem_size + cache->_alignment - 1)
    & ~(cache->_alignment - 1);

  // Compute number of elements fitting into a slab
  unsigned elem_num = 
    (cache->_slab_size - offset_first_elem) / entry_size;

  // Compute pointer to first data element, now taking into account
  // the latest colorization offset
  char* data = 
    reinterpret_cast<char*>(this) + offset_first_elem + cache->_latest_offset;

  // Update the colorization offset
  cache->_latest_offset += cache->_alignment;
  if (offset_first_elem + cache->_latest_offset + entry_size * elem_num 
      > cache->_slab_size)
    {
      cache->_latest_offset = 0;
    }

  // Initialize the cache elements
  slab_entry *e = 0, *e_prev = 0;

  for (unsigned i = 0; i < elem_num; i++)
    {
      e = reinterpret_cast<slab_entry *>(data);
      
      e->_next_free = e_prev;
      cache->elem_ctor(& e->_entry[0]);
      
      e_prev = e;
      
      data += 
	(sizeof(slab_entry) + cache->_elem_size + cache->_alignment - 1) 
	& ~(cache->_alignment - 1);
    }

  _data._first_free = e;
  _data._free = elem_num;
}

PUBLIC
void *
slab::alloc()
{
  slab_entry *e = _data._first_free;

  if (! e)
    return 0;

  _data._first_free = e->_next_free;
  _data._in_use ++;
  _data._free --;
    
  return & e->_entry;
}

PUBLIC
void
slab::free(void *entry)
{
  slab_entry *e = reinterpret_cast<slab_entry *>
    (reinterpret_cast<char*>(entry) - offsetof(slab_entry, _entry));

  e->_next_free = _data._first_free;
  _data._first_free = e;

  assert(_data._in_use);
  _data._in_use --;
  _data._free ++;
}

PUBLIC
inline bool
slab::is_empty()
{
  return _data._in_use == 0;
}

PUBLIC
inline bool
slab::is_full()
{
  return _data._free == 0;
}

PUBLIC
inline unsigned
slab::in_use()
{
  return _data._in_use;
}

PUBLIC
void
slab::enqueue(slab *prev)
{
  assert(prev);

  if ((_data._next = prev->_data._next))
    _data._next->_data._prev = this;
  _data._prev = prev;
  _data._prev->_data._next = this;
}

PUBLIC
void
slab::dequeue()
{
  if (_data._prev)
    _data._prev->_data._next = _data._next;
  if (_data._next)
    _data._next->_data._prev = _data._prev;

  _data._prev = _data._next = 0;
}

PUBLIC
inline slab *
slab::prev()
{
  return _data._prev;
}

PUBLIC
inline slab *
slab::next()
{
  return _data._next;
}

PUBLIC
inline void *
slab::operator new(size_t,
		   slab_cache_anon *cache) throw()
{
  // slabs must be size-aligned so that we can compute their addresses
  // from element addresses
  return cache->block_alloc(cache->_slab_size, cache->_slab_size);
}

PUBLIC static
unsigned
slab::num_elems(unsigned long slab_size,
    unsigned elem_size,
    unsigned alignment)
{
  // Compute offset of first slab_entry in slab, not taking into
  // account the colorization offset.  "slab_entry._entry[]" needs to
  // be "cache->_alignment"-aligned
  unsigned long offset_first_elem = 
    ((sizeof(slab::slab_data) + sizeof (slab::slab_entry) + alignment - 1) 
     & ~(alignment - 1)) 
    - sizeof (slab::slab_entry);

  // Compute size of a slab entry, including alignment padding at end
  // of entry
  unsigned entry_size = 
    (sizeof(slab::slab_entry) + elem_size + alignment - 1)
    & ~(alignment - 1);

  // Compute number of elements fitting into a slab
  return (slab_size - offset_first_elem) / entry_size;
}

PUBLIC static
unsigned
slab_cache_anon::num_elems(unsigned long slab_size,
    unsigned elem_size,
    unsigned alignment)
{ return slab::num_elems(slab_size, elem_size, alignment); }

// 
// slab_cache_anon
// 
PUBLIC inline
slab_cache_anon::slab_cache_anon(unsigned elem_size, 
				 unsigned alignment,
				 char const * name, 
				 unsigned long min_size,
				 unsigned long max_size)
  : _first_slab(0), _first_available_slab(0), _last_slab(0),
    _elem_size(elem_size), 
    _latest_offset(0), _alignment(alignment),
    _name (name)
{
  lock.init();

  for (
      _slab_size = min_size;
      num_elems(_slab_size, elem_size, alignment) < 8
        && _slab_size < max_size;
      _slab_size <<= 1) ;
}

// 
// slab_cache_anon
// 
PUBLIC inline
slab_cache_anon::slab_cache_anon(unsigned long slab_size, 
				 unsigned elem_size, 
				 unsigned alignment,
				 char const * name)
  : _first_slab(0), _first_available_slab(0), _last_slab(0),
    _slab_size(slab_size), _elem_size(elem_size), 
    _latest_offset(0), _alignment(alignment),
    _name (name)
{
  lock.init();
}

PUBLIC
virtual
slab_cache_anon::~slab_cache_anon()
{
  // the derived class should call destroy() before deleting us.
  // assert(_first_slab == 0);
}

PROTECTED inline
void
slab_cache_anon::destroy()	// descendant should call this in destructor
{
#if 0
  slab *n, *s = _first_slab;

  while (s)
    {
      n = s->next();

      // explicitly call destructor to delete s;
      s->~slab();
      block_free(s, _slab_size);

      s = n;
    }

  _first_slab = 0;
#endif
}

PUBLIC
virtual void *
slab_cache_anon::alloc()	// request initialized member from cache
{
  Lock_guard<Lock> guard(&lock);

  if (! _first_available_slab)
    {
      slab *s = new (this) slab(this);
      if (! s)
	return 0;

      _first_available_slab = s;
      
      if (_last_slab)
	{
	  assert(_last_slab->is_full());
	  
	  _first_available_slab->enqueue(_last_slab);
	  _last_slab = _first_available_slab;
	}
      else			// this was the first slab we allocated
	_first_slab = _last_slab = _first_available_slab;
    }

  assert(_first_available_slab 
	 && ! _first_available_slab->is_full());
  assert(! _first_available_slab->prev()
	 || _first_available_slab->prev()->is_full());

  void *ret = _first_available_slab->alloc();
  assert(ret);

  if (_first_available_slab->is_full())
    _first_available_slab = _first_available_slab->next();

  return ret;
}

PUBLIC template< typename Q >
inline
void *
slab_cache_anon::q_alloc(Q *quota)
{
  if (EXPECT_FALSE(!quota->alloc(_elem_size)))
    return 0;

  void *r;
  if (EXPECT_FALSE(!(r=alloc())))
    {
      quota->free(_elem_size);
      return 0;
    }

  return r;
}

PUBLIC
virtual void 
slab_cache_anon::free(void *cache_entry) // return initialized member to cache
{
  Lock_guard<Lock> guard(&lock);

  slab *s = reinterpret_cast<slab*>
    (reinterpret_cast<unsigned long>(cache_entry) & ~(_slab_size - 1));

  bool was_full = s->is_full();

  s->free(cache_entry);

  if (was_full)
    {
      if (s->next() == 0)	// have no right neighbor?
	{
	  assert(! _first_available_slab);
	}
      else if (s->next()->is_full()) // right neigbor full?
	{
	  // We requeue to become the first non-full slab in the queue
	  // so that all full slabs are at the beginning of the queue.

	  if (s == _first_slab)
	    _first_slab = s->next();
	  // don't care about _first_available_slab, _last_slab --
	  // they cannot point to s because we were full and have a
	  // right neighbor

	  s->dequeue();

	  if (_first_available_slab)
	    {
	      // _first_available_slab->prev() is defined because we
	      // had a right neighbor which is full, that is,
	      // _first_available_slab wasn't our right neighbor and
	      // now isn't the first slab in the queue
	      assert(_first_available_slab->prev()->is_full());
	      s->enqueue(_first_available_slab->prev());
	    }
	  else
	    {
	      // all slabs were full
	      assert(_last_slab->is_full());
	      s->enqueue(_last_slab);
	      _last_slab = s;
	    }
	}

      _first_available_slab = s;

    }
  else if (s->is_empty())
    {
      if (s->next() && (! s->next()->is_empty())) // right neighbor not empty?
	{
	  // Move to tail of list

	  if (s == _first_slab)
	    _first_slab = s->next();
	  if (s == _first_available_slab)
	    _first_available_slab = s->next();
	  // don't care about _last_slab because we know we have a
	  // right neighbor

	  s->dequeue();

	  s->enqueue(_last_slab);
	  _last_slab = s;
	}
    }
  else
    {
      // We weren't either full or empty; we already had free
      // elements.  This changes nothing in the queue, and there
      // already must have been a _first_available_slab.
    }

  assert(_first_available_slab);
}

PUBLIC template< typename Q >
inline
void
slab_cache_anon::q_free(Q *quota, void *obj)
{
  free(obj);
  quota->free(_elem_size);
}

PUBLIC
virtual unsigned long 
slab_cache_anon::reap()		// request that cache returns memory to system
{
  Lock_guard<Lock> guard(&lock);

  if (! _first_slab)
    return 0;			// haven't allocated anything yet

  // never delete first slab, even if it is empty
  if (_last_slab == _first_slab
      || (! _last_slab->is_empty()))
    return 0;

  slab *s = _last_slab;

  if (_first_available_slab == s)
    _first_available_slab = 0;

  _last_slab = s->prev();
  s->dequeue();

  // explicitly call destructor to delete s;
  s->~slab();
  block_free(s, _slab_size);

  return _slab_size;
}

// Element initialization/destruction
PROTECTED
virtual void 
slab_cache_anon::elem_ctor(void *)
{}

PROTECTED
virtual void 
slab_cache_anon::elem_dtor(void *)
{}

// Debugging output

#include <cstdio>

PUBLIC 
virtual void
slab_cache_anon::debug_dump ()
{
  printf ("%s: %lu-KB slabs (",
	  _name, _slab_size / 1024);

  unsigned count, total = 0, total_elems = 0;
  slab* s = _first_slab;

  for (count = 0;
       s && s->is_full();
       s = s->next())
    {
      count++;
      total_elems += s->in_use();
    }

  total += count;

  printf ("%u full, ", count);

  for (count = 0;
       s && ! s->is_empty();
       s = s->next())
    {
      if (s->is_full())
	printf ("\n*** wrongly-enqueued full slab found\n");
      count++;
      total_elems += s->in_use();
    }

  total += count;

  printf ("%u used, ", count);

  for (count = 0;
       s;
       s = s->next())
    {
      if (! s->is_empty())
	printf ("\n*** wrongly-enqueued nonempty slab found\n");
      count++;
      total_elems += s->in_use();
    }

  unsigned total_used = total;
  total += count;

  printf ("%u empty = %u total) = %lu KB,\n  %u elems (size=%u, align=%u)",
	  count, total, total * _slab_size / 1024, 
	  total_elems, _elem_size, _alignment);

  if (total_elems)
    printf (", overhead = %lu B (%lu B)  = %lu%% (%lu%%) \n", 
	    total * _slab_size - total_elems * _elem_size,
	    total_used * _slab_size - total_elems * _elem_size,
	    100 - total_elems * _elem_size * 100 / (total * _slab_size),
	    100 - total_elems * _elem_size * 100 / (total_used * _slab_size));
  else
    printf ("\n");
}
