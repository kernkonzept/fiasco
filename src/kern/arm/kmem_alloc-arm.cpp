IMPLEMENTATION [arm]:
// Kmem_alloc::Kmem_alloc() puts those Mem_region_map's on the stack which
// is slightly larger than our warning limit but it's on the boot stack only
// so this it is ok.
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include "mem_unit.h"
#include "ram_quota.h"

#include "mem_layout.h"
#include "kmem_space.h"
#include "static_init.h"

PRIVATE //inline
bool
Kmem_alloc::map_pmem(unsigned long phy, unsigned long size)
{
  static unsigned long next_map = Mem_layout::Pmem_start;
  size = Mem_layout::round_superpage(size + (phy & ~Config::SUPERPAGE_MASK));
  phy = Mem_layout::trunc_superpage(phy);

  if (next_map + size > Mem_layout::Pmem_end)
    return false;

  if (!Mem_layout::add_pmem(phy, next_map, size))
    return false;

  for (unsigned long i = 0; i <size; i += Config::SUPERPAGE_SIZE)
    {
      auto pte = Mem_layout::kdir->walk(Virt_addr(next_map + i),
                                        Kpdir::Super_level);
      assert (!pte.is_valid());
      assert (pte.page_order() == Config::SUPERPAGE_SHIFT);
      pte.set_page(pte.make_page(Phys_mem_addr(phy + i),
                                 Page::Attr(Page::Rights::RW(),
                                            Page::Type::Normal(),
                                            Page::Kern::Global())));
      pte.write_back_if(true);
      Mem_unit::kernel_tlb_flush((void *)(next_map + i));
    }

  next_map += size;
  return true;
}

PUBLIC inline NEEDS["kmem_space.h", "mem_layout.h"]
Address
Kmem_alloc::to_phys(void *v) const
{
  return Mem_layout::pmem_to_phys((Address)v);
}

static unsigned long _freemap[
  Kmem_alloc::Alloc::free_map_bytes(Mem_layout::Pmem_start,
                                    Mem_layout::Pmem_start + Config::KMEM_SIZE - 1)
  / sizeof(unsigned long)];

static_assert(Config::KMEM_SIZE <= Mem_layout::Pmem_end - Mem_layout::Pmem_start,
              "Kernel memory does not fit into Pmem range.");

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  // The -Wframe-larger-than= warning for this function is known and
  // no problem, because the function runs only on our boot stack.
  Mword alloc_size = Config::KMEM_SIZE;
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < (1 << 18))
    panic("Kmem_alloc: No kernel memory available (%ld)\n",
          available_size);

  a->init(Mem_layout::Pmem_start);
  a->setup_free_map(_freemap, sizeof(_freemap));

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];
      if (f.size() > alloc_size)
        f.start += (f.size() - alloc_size);

      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Reserved));
      if (0)
        printf("Kmem_alloc: [%08lx; %08lx] sz=%ld\n", f.start, f.end, f.size());
      if (Mem_layout::phys_to_pmem(f.start) == ~0UL)
        if (!map_pmem(f.start, f.size()))
          {
            WARN("Kmem_alloc: cannot map physical memory %p\n", (void*)f.start);
            break;
          }

      a->add_mem((void *)Mem_layout::phys_to_pmem(f.start), f.size());
      alloc_size -= f.size();
    }

  if (alloc_size)
    WARNX(Warning, "Kmem_alloc: cannot allocate sufficient kernel memory\n");
}

/**
 * Add initial phys->virt mapping of kernel image.
 *
 * Required to be called *before* any other function is called that needs to
 * know the phys-to-virt mapping. The function can rely on the fact that
 * Mem_layout::Map_base is always mapped with at least 4 MiB.
 */
static void add_initial_pmem()
{
  Mem_layout::add_pmem(Mem_layout::Sdram_phys_base, Mem_layout::Map_base,
                       4 << 20);
}

STATIC_INITIALIZER_P(add_initial_pmem, BOOTSTRAP_INIT_PRIO);

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && debug]:

#include <cstdio>

#include "panic.h"

PUBLIC
void Kmem_alloc::debug_dump()
{
  a->dump();

  unsigned long free = a->avail();
  printf("Used %ldKB out of %dKB of Kmem\n",
	 (Config::KMEM_SIZE - free + 1023)/1024,
	 (Config::KMEM_SIZE        + 1023)/1024);
}
