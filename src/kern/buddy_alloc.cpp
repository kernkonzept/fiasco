INTERFACE:

#include <cassert>
#include <cxx/hlist>
#include "bitmap.h"
#include "config.h"

class Buddy_base
{
protected:
  unsigned long _base;

  struct Head : cxx::H_list_item
  {
    unsigned long index;

    static void link(cxx::H_list<Head> &h, void *b, unsigned idx)
    {
      Head *n = (Head*)b;
      n->index = idx;
      h.add(n);
    }
  };

  typedef cxx::H_list_bss<Head> B_list;
};

template< int MIN_LOG2_SIZE, int NUM_SIZES, int MAX_MEM >
class Buddy_t_base : public Buddy_base
{
public:
  enum
  {
    Min_log2_size = MIN_LOG2_SIZE,
    Min_size = 1UL << MIN_LOG2_SIZE,
    Num_sizes = NUM_SIZES,
    Max_size = Min_size << (NUM_SIZES - 1),
    Max_mem = MAX_MEM,
  };

private:
  enum
  {
    // the number of bits in the bitmap is given by the amount of the smallest
    // supported blocks. We need an extra bit in the case that the Max_mem
    // is no multiple of Max_size to ensure that buddy() does not access
    // beyond the bitmap.
    Buddy_bits = (Max_mem + Min_size - 1)/Min_size
                 + !!(Max_mem & (Max_size-1))
  };
  B_list _free[Num_sizes];
  Bitmap<Buddy_bits> _free_map;
};


class Buddy_alloc : public Buddy_t_base<10, 8, Config::kernel_mem_max>
{
};


//----------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstdio>
#include <cassert>
#include "warn.h"

PRIVATE
template<int A, int B, int M> 
inline
Buddy_base::Head *
Buddy_t_base<A,B,M>::buddy(void *block, unsigned long index, Head **new_block)
{
  //printf("buddy(%p, %ld)\n", block, index);
  unsigned long const size = Min_size << index;
  unsigned long const n_size = size << 1;
  if (index + 1 >= Num_sizes)
    return 0;
  unsigned long b = (unsigned long)block;
  unsigned long _buddy = b & ~(n_size-1);
  *new_block = (Head*)_buddy;
  if (_buddy == b)
    _buddy += size;

  Head * const _buddy_h = (Head*)_buddy;

  // this test may access one bit behind our maximum, this is safe because
  // we allocated an extra bit
  if (_free_map[(_buddy - _base)/Min_size] && _buddy_h->index == index)
    return _buddy_h;

  return 0;
}

PUBLIC
template<int A, int B, int M>
inline
void
Buddy_t_base<A,B,M>::free(void *block, unsigned long size)
{
  assert ((unsigned long)block >= _base);
  assert ((unsigned long)block - _base < Max_mem);
  assert (!_free_map[((unsigned long)block - _base) / Min_size]);
  //bool _b = 0;
  //if (_debug) printf("Buddy::free(%p, %ld)\n", block, size);
  unsigned size_index = 0;
  while (((unsigned long)Min_size << size_index) < size)
    ++size_index;

  if (size != (unsigned long)Min_size << size_index)
    WARNX(Info, "Buddy::free: Size mismatch: %lx v %lx\n",
          size, (unsigned long)Min_size << size_index);


  // no need to look for a buddy if we already have the biggest block size
  while (size_index + 1 < Num_sizes)
    {
      Head *n, *b;
      b = buddy(block, size_index, &n);
      if (b)
	{
	//if (!_b && _debug) dump();
	//if (_debug) printf("  found buddy %p (n=%p size=%ld)\n", b, n, size_index+1);
	  B_list::remove(b);
	  block = n;
	  ++size_index;
	  //_b = 1;
	}
      else
	break;
    }

  //printf("  link free %p\n", block);
  Head::link(_free[size_index], block, size_index);
  _free_map.set_bit(((unsigned long)block - _base) / Min_size);
  //if (_b && _debug) dump();
}


PUBLIC
template<int A, int B, int M>
void
Buddy_t_base<A,B,M>::add_mem(void *b, unsigned long size)
{
  unsigned long start = (unsigned long)b;
  unsigned long al_start;
  al_start = (start + Min_size - 1) & ~(Min_size - 1);

  //printf("Buddy::add_mem(%p, %lx): al_start=%lx; _base=%lx\n", b, size, al_start, _base);

  // _debug = 0;
  if (size <= al_start - start)
    return;

  size -= al_start - start;
  size &= ~(Min_size - 1);

  while (size)
    {
      free((void*)al_start, Min_size);
      al_start += Min_size;
      size -= Min_size;
    }
  // _debug = 1;
  //dump();
}



PRIVATE
template<int A, int B, int M>
inline
void
Buddy_t_base<A,B,M>::split(Head *b, unsigned size_index, unsigned i)
{
  //unsigned si = size_index;
  //printf("Buddy::split(%p, %d, %d)\n", b, size_index, i);
  for (; i > size_index; ++size_index)
    {
      unsigned long buddy = (unsigned long)b + (Min_size << size_index);
      Head::link(_free[size_index], (void*)buddy, size_index);
      _free_map.set_bit((buddy - _base) / Min_size);
    }

  //if (si!=i) dump();
}

PUBLIC
template<int A, int B, int M>
inline
void *
Buddy_t_base<A,B,M>::alloc(unsigned long size)
{
  unsigned size_index = 0;
  while (((unsigned long)Min_size << size_index) < size)
    ++size_index;

  if (size != (unsigned long)Min_size << size_index)
    WARNX(Info, "Buddy::alloc: Size mismatch: %lx v %lx\n",
          size, (unsigned long)Min_size << size_index);

  //printf("[%u]: Buddy::alloc(%ld)[ret=%p]: size_index=%d\n", Proc::cpu_id(), size, __builtin_return_address(0), size_index);

  for (unsigned i = size_index; i < Num_sizes; ++i)
    {
      Head *f = _free[i].front();
      if (f)
	{
	  B_list::remove(f);
	  split(f, size_index, i);
	  _free_map.clear_bit(((unsigned long)f - _base) / Min_size);
	  //printf("[%u]: =%p\n", Proc::cpu_id(), f);
	  return f;
	}
    }
  return 0;
}

PUBLIC
template< int A, int B, int M >
void
Buddy_t_base<A,B,M>::dump() const
{
  unsigned long total = 0;
  printf("Buddy_alloc [%d,%d]\n", Min_size, Num_sizes);
  for (unsigned i = 0; i < Num_sizes; ++i)
    {
      unsigned c = 0;
      unsigned long avail = 0;
      B_list::Const_iterator h = _free[i].begin();
      printf("  [%d] %p(%lu)", Min_size << i, *h, h != _free[i].end() ? h->index : 0UL);
      while (h != _free[i].end())
	{
	  ++h;
	  if (c < 5)
	    printf(" -> %p(%lu)", *h, *h?h->index:0UL);
	  else if (c == 5)
            printf(" ...");

          if (*h)
            avail += Min_size << i;

	  ++c;
	}
      printf(" == %luK (%lu)\n", avail / 1024, avail);
      total += avail;
    }

  printf("sum of available memory: %luK (%lu)\n", total / 1024, total);
}

PUBLIC
void
Buddy_base::init(unsigned long base)
{ _base = base; }

PUBLIC
template< int A, int B, int M >
unsigned long
Buddy_t_base<A,B,M>::avail() const
{
  unsigned long a = 0;
  for (unsigned i = 0; i < Num_sizes; ++i)
    {
      for (B_list::Const_iterator h = _free[i].begin(); h != _free[i].end(); ++h)
        a += (Min_size << i);
    }
  return a;
}

