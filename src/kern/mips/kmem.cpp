INTERFACE [mips32]:

#include "mem_layout.h"
#include "paging.h"

class Kmem : public Mem_layout
{
public:
  static bool is_kmem_page_fault(Mword pfa, Mword /*cause*/)
  { return pfa >= 0x80000000; }

  static bool is_io_bitmap_page_fault(Mword)
  { return false; }

  // currently a dummy the kernel runs unpaged
  static Pdir *const kdir;
};

INTERFACE [mips64]:

#include "mem_layout.h"
#include "paging.h"

class Kmem : public Mem_layout
{
public:
  static bool is_kmem_page_fault(Mword pfa, Mword /*cause*/)
  { return pfa >= 0x8000000000000000; }

  static bool is_io_bitmap_page_fault(Mword)
  { return false; }

  // currently a dummy the kernel runs unpaged
  static Pdir *const kdir;
};

IMPLEMENTATION [mips]:

#include <cassert>

Pdir *const Kmem::kdir = 0;

PUBLIC static
Address
Kmem::mmio_remap(Address phys)
{
  assert ((phys < 0x20000000) && "MMIO outside KSEG1");
  return phys + KSEG1;
}
