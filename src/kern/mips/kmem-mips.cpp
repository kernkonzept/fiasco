IMPLEMENTATION [mips32]:

IMPLEMENT inline
bool
Kmem::is_kmem_page_fault(Mword pfa, Mword /*cause*/)
{ return pfa >= 0x80000000; }

IMPLEMENTATION [mips64]:

IMPLEMENT inline
bool
Kmem::is_kmem_page_fault(Mword pfa, Mword /*cause*/)
{ return pfa >= 0x8000000000000000; }

IMPLEMENTATION [mips]:

#include <cassert>

// currently a dummy, the kernel runs unpaged
Kpdir *Kmem::kdir = nullptr;

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size)
{
  (void)size;
  assert ((phys + size < 0x20000000) && "MMIO outside KSEG1");
  return phys + KSEG1;
}
