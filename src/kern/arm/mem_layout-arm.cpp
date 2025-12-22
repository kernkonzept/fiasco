INTERFACE [arm && mmu]:

#include "config.h"

EXTENSION class Mem_layout
{
private:
  // At least two entries are expected: the kernel image and the heap. If the
  // RAM is not contiguous there might be more than one heap region needed,
  // though.
  enum { Max_pmem_regions = 8 };

  struct Pmem_region
  {
    Address paddr;
    Address vaddr;
    unsigned long size;
  };

  static Pmem_region _pm_regions[Max_pmem_regions];
  static unsigned _num_pm_regions;
};

// -------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu]:

Mem_layout::Pmem_region Mem_layout::_pm_regions[Mem_layout::Max_pmem_regions];
unsigned Mem_layout::_num_pm_regions;

IMPLEMENT static
Address
Mem_layout::phys_to_pmem(Address phys)
{
  for (unsigned i = 0; i < _num_pm_regions; i++)
    {
      if (   phys >= _pm_regions[i].paddr
          && phys <= _pm_regions[i].paddr + _pm_regions[i].size)
        return phys - _pm_regions[i].paddr + _pm_regions[i].vaddr;
    }

  return ~0UL;
}

IMPLEMENT static
Address
Mem_layout::pmem_to_phys(Address virt)
{
  for (unsigned i = 0; i < _num_pm_regions; i++)
    {
      if (   virt >= _pm_regions[i].vaddr
          && virt <= _pm_regions[i].vaddr + _pm_regions[i].size)
        return virt - _pm_regions[i].vaddr + _pm_regions[i].paddr;
    }

  return ~0UL;
}

PUBLIC static
bool
Mem_layout::add_pmem(Address phys, Address virt, unsigned long size)
{
  if (size == 0)
    return false;

  if (_num_pm_regions >= Max_pmem_regions)
    return false;

  size--;

  // The pmem map must be unambiguous in either direction. Make sure nothing
  // intersects with existing records.
  for (unsigned i = 0; i < _num_pm_regions; i++)
    {
      if (   phys + size >= _pm_regions[i].paddr
          && phys        <= _pm_regions[i].paddr + _pm_regions[i].size)
        return false;

      if (   virt + size >= _pm_regions[i].vaddr
          && virt        <= _pm_regions[i].vaddr + _pm_regions[i].size)
        return false;
    }

  _pm_regions[_num_pm_regions].paddr = phys;
  _pm_regions[_num_pm_regions].vaddr = virt;
  _pm_regions[_num_pm_regions].size = size;
  _num_pm_regions++;

  return true;
}

// -------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu]:

IMPLEMENT static
Address
Mem_layout::phys_to_pmem(Address phys)
{
  return phys;
}

IMPLEMENT static
Address
Mem_layout::pmem_to_phys(Address virt)
{
  return virt;
}
