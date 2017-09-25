//----------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include "mem_region.h"
IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  Mword alloc_size = Config::Kmem_size;
  unsigned long max = ~0UL;
#warning This code needs adaption (see e.g. the arm version)
  for (;;)
    {
      Mem_region r; r.start=3; r.end=1; // = Kip::k()->last_free(max);

      if (r.start > r.end + 1)
        panic("Corrupt memory descscriptor in KIP...");

      if (r.start == r.end + 1)
        panic("Could not acquire enough kernel memory");

      max = r.start;
      Mword size = r.end - r.start + 1;
      if(alloc_size <= size)
	{
	  r.start += (size - alloc_size);
	  Kip::k()->add_mem_region(Mem_desc(r.start, r.end,
		                            Mem_desc::Reserved));

	  /* init buddy allocator with physical addresses */
	  a->init(r.start);
	  a->add_mem((void*)r.start, alloc_size);
	  printf("Buddy allocator at: [%08lx; %08lx] - %lu KB\n", 
	         r.start, r.end, alloc_size / 1024);
	  break;
	}
    }
}

PUBLIC inline //NEEDS["kmem_space.h"]
Address
Kmem_alloc::to_phys(void *v) const
{
  (void)v;
  //return Kmem_space::kdir()->virt_to_phys((Address)v);
  return ~0UL;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [ppc32 && debug]:

PUBLIC
void Kmem_alloc::debug_dump()
{
  a->dump();
}
