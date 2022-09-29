INTERFACE:

#include <cstddef>
#include <cxx/slist>
#include <cxx/type_traits>
#include "types.h"
#include "per_node_data.h"

class Boot_alloced
{
private:
  enum { Debug_boot_alloc };
  struct Block : cxx::S_list_item
  {
    size_t size;

    inline Block *next()
    {
      return (Block *)((Address)this + size);
    }
  };

  typedef cxx::S_list_bss<Block> Block_list;

  static Per_node_data<Block_list> _free;
};

template< typename Base >
class Boot_object : public Base, public Boot_alloced
{
public:
  Boot_object()  = default;

  template< typename... A >
  Boot_object(A&&... args) : Base(cxx::forward<A>(args)...) {}
};


IMPLEMENTATION:

#include <cstdio>
#include <cstring>

#include "kmem_alloc.h"
#include "warn.h"

DECLARE_PER_NODE Per_node_data<Boot_alloced::Block_list> Boot_alloced::_free;

PUBLIC static
void *
Boot_alloced::alloc(size_t size)
{
  if (Debug_boot_alloc)
    printf("Boot_alloc: size=0x%lx\n", (unsigned long)size);

  // this is best fit list-based allocation

  Block_list::Iterator best = _free->end();
  for (Block_list::Iterator curr = _free->begin(); curr != _free->end(); ++curr)
    {
      if (((best == _free->end()) || curr->size < best->size)
	  && curr->size >= size)
	best = curr;
    }

  if (best == _free->end())
    {
      // start from 1k
      unsigned long alloc_size = 1024;

      // look for a size suitable and buddy friendly
      while (alloc_size < size)
	alloc_size <<= 1;

      Block *b = (Block*)Kmem_alloc::allocator()->alloc(Bytes(alloc_size));
      if (Debug_boot_alloc)
        printf("Boot_alloc: allocated extra memory block @%p (size=%lx)\n",
               b, alloc_size);

      if (!b)
	return 0;

      best = free_block(alloc_size, b);
    }

  void *b = *best;
  Block *rem = (Block *)(((Address)b + size + sizeof(Block) - 1) & ~(sizeof(Block) - 1));
  long rem_sz = (Address)b + (*best)->size - (Address)rem;
  if (Debug_boot_alloc)
    printf("Boot_alloc: @ %p\n", b);
  if (rem_sz > (long)sizeof(Block))
    {
      rem->size = rem_sz;
      _free->replace(best, rem);
      if (Debug_boot_alloc)
        printf("Boot_alloc: remaining free block @ %p (size=%lx)\n", rem, (unsigned long)rem_sz);
    }
  else
    _free->erase(best);

  memset(b, 0, size);
  return b;
}

/**
 * Insert a free block into the free list.
 *
 * The list is kept sorted by address to make merging free blocks possible. We
 * may want to pass back complete buddies back to the buddy allocator in the
 * future.
 *
 * \pre The block pointer and size must be sizeof(Block) aligned.
 */
PRIVATE static
Boot_alloced::Block_list::Iterator
Boot_alloced::free_block(unsigned long size, Block *b)
{
  b->size = size;

  Block_list::Iterator prev = _free->end();
  Block_list::Iterator next = _free->begin();
  while (next != _free->end())
    {
      if (*next < b)
	break;
      prev = next;
      ++next;
    }

  if (prev == _free->end())
    {
      if (next != _free->end() && *next == b->next())
	{
	  b->size += next->size;
	  _free->replace(next, b);
	}
      else
        _free->add(b);

      return _free->begin();
    }
  else
    {
      // Merge with 'next' block first (if adjacent) to keep 'prev' iterator
      // valid!
      if (next != _free->end() && *next == b->next())
	{
	  b->size += next->size;
	  _free->erase(next);
	}

      if (prev->next() == b)
	{
	  prev->size += b->size;
	  return prev;
	}
      else
	{
	  _free->insert(b, prev);
	  return ++prev;
	}
    }
}

PUBLIC static
void
Boot_alloced::free(unsigned long size, void *b)
{
  if (!size || !b)
    return;

  size = (size + sizeof(Block) - 1U) & ~(sizeof(Block) - 1U);
  free_block(size, (Block *)b);
}

PUBLIC template<typename T> static
T *
Boot_alloced::allocate(size_t count = 1)
{
  return reinterpret_cast<T *>(alloc(count * sizeof(T)));
}

PUBLIC inline void *
Boot_alloced::operator new (size_t size) throw()
{ return alloc(size); }

PUBLIC inline void *
Boot_alloced::operator new [] (size_t size) throw()
{ return alloc(size); }

PUBLIC void
Boot_alloced::operator delete (void *b)
{
  WARN("Boot_alloc: trying to delete boot-time allocated object @ %p\n", b);
}

PUBLIC void
Boot_alloced::operator delete [] (void *b)
{
  WARN("Boot_alloc: trying to delete boot-time allocated object @ %p\n", b);
}

