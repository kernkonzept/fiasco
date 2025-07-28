INTERFACE:

#include <cassert>
#include <cstring>
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

    static void link(cxx::H_list<Head> &h, void *b, unsigned long idx)
    {
      Head *n = static_cast<Head*>(b);
      n->index = idx;
      h.add(n);
    }
  };

  typedef cxx::H_list_bss<Head> B_list;
};

template< unsigned long MIN_LOG2_SIZE, int NUM_SIZES>
class Buddy_t_base : public Buddy_base
{
  enum { Debug_size_mismatch = 0 };

  template <unsigned long, int>
  friend class Buddy_t_base_tester;

private:
  struct Freemap : Bitmap_storage<unsigned long *>
  {
    void setup(unsigned long *addr, size_t size_in_bytes)
    {
      _bits = addr;
      memset(_bits, 0, size_in_bytes);
    }
  };

public:
  enum : unsigned long
  {
    Min_log2_size = MIN_LOG2_SIZE,
    Min_size = 1UL << MIN_LOG2_SIZE,
    Num_sizes = NUM_SIZES,
    Max_size = Min_size << (NUM_SIZES - 1),
  };

  static constexpr size_t
  free_map_bytes(unsigned long min_addr, unsigned long max_addr)
  { return Freemap::size_in_bytes(buddy_bits(min_addr, max_addr)); }

  static constexpr unsigned long
  free_map_align()
  { return alignof (unsigned long); }

  void setup_free_map(unsigned long *addr, size_t size_in_bytes)
  { _free_map.setup(addr, size_in_bytes); }

  static constexpr unsigned long
  calc_base_addr(unsigned long min_addr)
  { return min_addr & ~(Max_size - 1); }

private:

  /**
   * Calculate the size in bytes that needs to be handled by
   * the free block bitmap.
   * \param min_addr  The minimum address of any memory to be handled
   *                  by the buddy allocator.
   * \param max_addr  The maximum address (inclusive) of any memory
   *                  block handled by the buddy allocator.
   * \returns The size in bytes of memory that must be handled by the buddy's
   *          free-blocks bitmap, taking any alignment constraints into
   *          account.
   */
  static constexpr unsigned long
  calc_max_mem_size(unsigned long min_addr, unsigned long max_addr)
  {
    return max_addr - calc_base_addr(min_addr) + 1;
  }

  static constexpr unsigned long
  buddy_bits(unsigned long min_addr, unsigned long max_addr)
  {
    // the number of bits in the bitmap is given by the amount of the smallest
    // supported blocks. We need an extra bit in the case that the Max_mem
    // is no multiple of Max_size to ensure that buddy() does not access
    // beyond the bitmap.
    return (calc_max_mem_size(min_addr, max_addr) + Min_size - 1) / Min_size
            + !!(calc_max_mem_size(min_addr, max_addr) & (Max_size - 1));
  }

  B_list _free[Num_sizes];
  Freemap _free_map;
};


class Buddy_alloc : public Buddy_t_base<10, 11>
{
};


//----------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstdio>
#include <cassert>
#include "warn.h"

PRIVATE
template<unsigned long A, int B>
inline
Buddy_base::Head *
Buddy_t_base<A,B>::buddy(void *block, unsigned long index, Head **new_block)
{
  //printf("buddy(%p, %ld)\n", block, index);
  unsigned long const size = Min_size << index;
  unsigned long const n_size = size << 1;
  if (index + 1 >= Num_sizes)
    return nullptr;
  unsigned long b = reinterpret_cast<unsigned long>(block);
  unsigned long _buddy = b & ~(n_size-1);
  *new_block = reinterpret_cast<Head*>(_buddy);
  if (_buddy == b)
    _buddy += size;

  Head * const _buddy_h = reinterpret_cast<Head*>(_buddy);

  // this test may access one bit behind our maximum, this is safe because
  // we allocated an extra bit
  if (_free_map[(_buddy - _base)/Min_size] && _buddy_h->index == index)
    return _buddy_h;

  return nullptr;
}

PUBLIC
template<unsigned long A, int B>
inline
void
Buddy_t_base<A,B>::free(void *block, unsigned long size)
{
  assert (reinterpret_cast<unsigned long>(block) >= _base);
  //assert ((unsigned long)block - _base < Max_mem);
  assert (!_free_map[(reinterpret_cast<unsigned long>(block) - _base)
                     / Min_size]);
  //bool _b = 0;
  //if (_debug) printf("Buddy::free(%p, %ld)\n", block, size);
  unsigned size_index = 0;
  while ((static_cast<unsigned long>(Min_size) << size_index) < size)
    ++size_index;

  if (Debug_size_mismatch
      && size != static_cast<unsigned long>(Min_size) << size_index)
    WARNX(Info, "Buddy::free: Size mismatch: %lx v %lx [%p]\n",
          size, static_cast<unsigned long>(Min_size) << size_index,
          __builtin_return_address(0));

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
  _free_map.set_bit((reinterpret_cast<unsigned long>(block) - _base) / Min_size);
  //if (_b && _debug) dump();
}


PUBLIC
template<unsigned long A, int B>
void
Buddy_t_base<A,B>::add_mem(void *b, unsigned long size)
{
  unsigned long start = reinterpret_cast<unsigned long>(b);
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
      free(reinterpret_cast<void*>(al_start), Min_size);
      al_start += Min_size;
      size -= Min_size;
    }
  // _debug = 1;
  //dump();
}



PRIVATE
template<unsigned long A, int B>
inline
void
Buddy_t_base<A,B>::split(Head *b, unsigned size_index, unsigned i)
{
  //unsigned si = size_index;
  //printf("Buddy::split(%p, %d, %d)\n", b, size_index, i);
  for (; i > size_index; ++size_index)
    {
      unsigned long buddy = reinterpret_cast<unsigned long>(b)
                            + (Min_size << size_index);
      Head::link(_free[size_index], reinterpret_cast<void*>(buddy), size_index);
      _free_map.set_bit((buddy - _base) / Min_size);
    }

  //if (si!=i) dump();
}

PUBLIC
template<unsigned long A, int B>
inline
void *
Buddy_t_base<A,B>::alloc(unsigned long size)
{
  unsigned size_index = 0;
  while ((static_cast<unsigned long>(Min_size) << size_index) < size)
    ++size_index;

  if (Debug_size_mismatch
      && size != static_cast<unsigned long>(Min_size) << size_index)
    WARNX(Info, "Buddy::alloc: Size mismatch: %lx v %lx [%p]\n",
          size, static_cast<unsigned long>(Min_size) << size_index,
          __builtin_return_address(0));

  //printf("[%u]: Buddy::alloc(%ld)[ret=%p]: size_index=%d\n", Proc::cpu_id(), size, __builtin_return_address(0), size_index);

  for (unsigned i = size_index; i < Num_sizes; ++i)
    {
      Head *f = _free[i].front();
      if (f)
	{
	  B_list::remove(f);
	  split(f, size_index, i);
	  _free_map.clear_bit((reinterpret_cast<unsigned long>(f) - _base)
                        / Min_size);
	  //printf("[%u]: =%p\n", Proc::cpu_id(), f);
	  return f;
	}
    }
  return nullptr;
}

PUBLIC
template< unsigned long A, int B>
void
Buddy_t_base<A,B>::dump() const
{
  unsigned long total = 0;
  printf("Buddy_alloc [%ld,%ld]\n", Min_size, Num_sizes);
  for (unsigned i = 0; i < Num_sizes; ++i)
    {
      unsigned long c = 0;
      unsigned long avail = 0;
      B_list::Const_iterator h = _free[i].begin();
      printf("  [%ld] %p(%lu)", Min_size << i,
             static_cast<void *>(*h), h != _free[i].end() ? h->index : 0UL);
      while (h != _free[i].end())
	{
	  ++h;
	  if (c < 5)
	    printf(" -> %p(%lu)", static_cast<void *>(*h), *h?h->index:0UL);
	  else if (c == 5)
            printf(" ...");

          avail += Min_size << i;

	  ++c;
	}
      printf(" == %luK (%lu)\n", avail / 1024, avail);
      total += avail;
    }

  printf("sum of available memory: %lu KB (%lu bytes)\n", total / 1024, total);
}

PUBLIC
template< unsigned long A, int B>
void
Buddy_t_base<A,B>::init(unsigned long base)
{ _base = calc_base_addr(base); }

PUBLIC
template< unsigned long A, int B>
unsigned long
Buddy_t_base<A,B>::avail() const
{
  unsigned long a = 0;
  for (unsigned i = 0; i < Num_sizes; ++i)
    {
      for (B_list::Const_iterator h = _free[i].begin(); h != _free[i].end(); ++h)
        a += (Min_size << i);
    }
  return a;
}

