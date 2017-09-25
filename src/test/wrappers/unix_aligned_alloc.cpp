INTERFACE:

IMPLEMENTATION:

#include <cstdlib>
#include <map>

static std::map<unsigned, void*> _free_map;

void *
unix_aligned_alloc(unsigned long size, unsigned long alignment)
{
  unsigned current_size = size;
  void* addr = malloc (current_size);
  unsigned aligned_addr = aligned(reinterpret_cast<unsigned>(addr), alignment);

  while (aligned_addr + size 
	 > reinterpret_cast<unsigned>(addr) + current_size)
    {
      current_size += 4096;
      addr = realloc (addr, current_size);
      aligned_addr = aligned(reinterpret_cast<unsigned>(addr), alignment);
    } 


  _free_map [aligned_addr] = addr;

  return reinterpret_cast<void*>(aligned_addr);
}

void 
unix_aligned_free(void *block, unsigned long size)
{
  free (_free_map[reinterpret_cast<unsigned>(block)]);
  _free_map.erase (reinterpret_cast<unsigned>(block));
}

static inline 
unsigned
aligned (unsigned addr, unsigned alignment)
{
  return (addr + alignment - 1) & ~(alignment - 1);
}
