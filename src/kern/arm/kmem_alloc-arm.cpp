IMPLEMENTATION [arm]:
// Kmem_alloc::Kmem_alloc() puts those Mem_region_map's on the stack which
// is slightly larger than our warning limit but it's on the boot stack only
// so this it is ok.
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include "mem_unit.h"
#include "ram_quota.h"

#include "mem_layout.h"
#include "kmem.h"
#include "kmem_space.h"
#include "minmax.h"
#include "static_init.h"
#include "paging_bits.h"

PRIVATE //inline
bool
Kmem_alloc::map_pmem(unsigned long phy, unsigned long size)
{
  static unsigned long next_map = Mem_layout::Pmem_start;

  assert(Super_pg::aligned(phy));
  assert(Super_pg::aligned(size));

  if (next_map + size > Mem_layout::Pmem_end)
    return false;

  if (!Mem_layout::add_pmem(phy, next_map, size))
    return false;

  for (unsigned long i = 0; i <size; i += Config::SUPERPAGE_SIZE)
    {
      auto pte = Kmem::kdir->walk(Virt_addr(next_map + i), Kpdir::Super_level);
      assert (!pte.is_valid());
      assert (pte.page_order() == Config::SUPERPAGE_SHIFT);
      pte.set_page(pte.make_page(Phys_mem_addr(phy + i),
                                 Page::Attr::kern_global(Page::Rights::RW())));
      pte.write_back_if(true);
      Mem_unit::tlb_flush_kernel(next_map + i);
    }

  next_map += size;
  return true;
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
  static_assert(Super_pg::aligned(Config::KMEM_SIZE),
                "KMEM_SIZE must be superpage-aligned");
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map,
                                                 Config::SUPERPAGE_SIZE);

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
      if (!map_pmem(f.start, f.size()))
        {
          WARN("Kmem_alloc: cannot map heap memory [%08lx; %08lx]\n",
               f.start, f.end);
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
 * know the phys-to-virt mapping. The function can rely on the fact that the
 * Fiasco bootstrap has created a 1:1 mapping when enabling paging which is
 * still around.
 */
static void add_initial_pmem()
{
  extern char _kernel_image_start[];
  extern char _initcall_end[];

  // Provides phys-to-virt address mapping required to walk the page table
  // hierarchy, which is linked via physical addresses.
  // Relies on 1:1 mapping of the kernel image, or rather that the pages for
  // initial/boot paging directory lie in the kernel image (see
  // kmem_space-arm-32/64.cpp).
  struct Identity_map
  {
    static Address phys_to_pmem(Address a)
    { return a; }
  };

  // Find out our virt->phys mapping simply by walking the page table.
  Address virt = Super_pg::trunc((Address)_kernel_image_start);
  Address size = Super_pg::round((Address)_initcall_end) - virt;
  auto pte = Kmem::kdir->walk(Virt_addr(virt), Kpdir::Super_level, false,
                              Ptab::Null_alloc(), Identity_map());
  assert(pte.is_valid());
  unsigned long phys = pte.page_addr();
  Mem_layout::add_pmem(phys, virt, size);

  // The existing 1:1 mapping is *not* removed! In case of cpu_virt on arm32 it
  // is required to exist for Mem_op::arm_mem_cache_maint().
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
  printf("Used %lu KiB out of %lu KiB of Kmem\n",
         (Config::KMEM_SIZE - free + 1023) / 1024,
         (Config::KMEM_SIZE        + 1023) / 1024);
}
