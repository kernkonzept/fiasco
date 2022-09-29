INTERFACE:

#include "mem.h"
#include "mem_unit.h"

#include <cassert>

class Mem_chunk
{
public:
  Mem_chunk() = default;

  inline Mem_chunk(void *va, unsigned size, unsigned alloc_size)
  : _va(va), _size(size), _alloc_size(alloc_size)
  {}

  inline bool is_valid() const
  { return _va != nullptr; }

  inline Address virt_addr() const
  { return reinterpret_cast<Address>(_va); }

  template<typename T = void>
  inline T *virt_ptr() const
  { return static_cast<T *>(_va); }

  inline Address phys_addr() const
  { return to_phys(virt_addr()); }

  unsigned size() const
  { return _size; }

  inline void free()
  {
    if (is_valid())
      {
        free_mem(_va, _alloc_size);
        _va = nullptr;
        _size = 0;
      }
  }

private:
  void *_va = nullptr;
  unsigned _size = 0;
  unsigned _alloc_size = 0;
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "buddy_alloc.h"
#include "kmem.h"
#include "kmem_alloc.h"

#include <arithmetic.h>
#include <cstring>
#include <minmax.h>

/**
 * Allocate uninitialized memory with alignment requirements.
 *
 * \param size   Size of the memory to allocate in bytes.
 * \param align  Alignment requirement of the memory to allocate, must be a
 *               power of two. The default alignment is defined as `size`
 *               rounded to next largest power of two.
 */
PUBLIC template<typename MEM = Mem_chunk> static
MEM
Mem_chunk::alloc_mem(unsigned size, unsigned align = 1)
{
  assert(size >= Kmem_alloc::Alloc::Min_size);
  assert(align == (1u << cxx::log2u(align)));

  // Underlying buddy allocator can only provide naturally aligned blocks of
  // power-of-two in size. Thus, to ensure proper alignment, we may have to
  // allocate more memory than requested.
  unsigned alloc_size = max(size, align);
  void *mem = Kmem_alloc::allocator()->alloc(Bytes(alloc_size));
  assert(reinterpret_cast<Address>(mem) % align == 0);
  return MEM(mem, size, alloc_size);
}

/**
 * Allocate zero initialized memory with alignment requirements.
 *
 * \param size   Size of the memory to allocate in bytes.
 * \param align  Alignment requirement of the memory to allocate, must be a
 *               power of two. The default alignment is defined as `size`
 *               rounded to next largest power of two.
 */
PUBLIC template<typename MEM = Mem_chunk> static
MEM
Mem_chunk::alloc_zmem(unsigned size, unsigned align = 1)
{
  MEM mem = alloc_mem(size, align);
  if (mem.is_valid())
    memset(mem.virt_ptr(), 0, size);
  return mem;
}

PRIVATE static
void
Mem_chunk::free_mem(void *mem, unsigned size)
{
  Kmem_alloc::allocator()->free(Bytes(size), mem);
}

PUBLIC static
Address
Mem_chunk::to_phys(Address virt)
{
  return (*Kmem::kdir)->virt_to_phys(virt);
}
