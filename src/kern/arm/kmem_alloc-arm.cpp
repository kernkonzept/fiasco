IMPLEMENTATION [arm]:
// Kmem_alloc::Kmem_alloc() puts those Mem_region_map's on the stack which
// is slightly larger than our warning limit but it's on the boot stack only
// so this it is ok.
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include "mem_unit.h"
#include "ram_quota.h"

#include "mem_layout.h"
#include "kmem.h"
#include "minmax.h"
#include "static_init.h"
#include "paging_bits.h"
#include "panic.h"

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu]:

PRIVATE
bool
Kmem_alloc::map_pmem(unsigned long phys, unsigned long size,
                     unsigned long *map_addr)
{
  static unsigned long next_map = Mem_layout::Pmem_start;

  assert(Super_pg::aligned(phys));
  assert(Super_pg::aligned(size));

  if (next_map + size > Mem_layout::Pmem_end)
    return false;

  *map_addr = next_map;
  if (!Mem_layout::add_pmem(phys, next_map, size))
    return false;

  for (unsigned long i = 0; i <size; i += Config::SUPERPAGE_SIZE)
    {
      auto pte = Kmem::kdir->walk(Virt_addr(next_map + i), Kpdir::super_level());
      assert(!pte.is_valid());
      assert(pte.page_order() == Config::SUPERPAGE_SHIFT);
      pte.set_page(Phys_mem_addr(phys + i),
                   Page::Attr::kern_global(Page::Rights::RW()));
      pte.write_back_if(true);
      Mem_unit::tlb_flush_kernel(next_map + i);
    }

  next_map += size;
  return true;
}

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  Free_regions_map map;
  unsigned long available_size
    = create_free_map(Kip::k(), &map, Config::SUPERPAGE_SIZE);

  unsigned long alloc_size = determine_kmem_alloc_size(available_size,
                                                       Config::SUPERPAGE_SIZE);

  // Walk through all KIP memory regions of conventional memory minus the
  // reserved memory and find one or more regions suitable for the kernel
  // memory.
  //
  // The kernel memory regions are added to the KIP as `Kernel_tmp`. Later, in
  // setup_kmem_from_kip_md_tmp(), these regions are added as kernel memory
  // (a()->add_mem()) and marked as "Reserved".
  unsigned long min_virt = ~0UL, max_virt = 0UL;
  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];
      if (f.size() > alloc_size)
        f.start += f.size() - alloc_size;

      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Kernel_tmp));

      unsigned long map_addr;
      if (!map_pmem(f.start, f.size(), &map_addr))
        panic("Kmem_alloc: cannot map heap memory [%08lx; %08lx]",
              f.start, f.end);

      min_virt = min(min_virt, map_addr);
      max_virt = max(max_virt, map_addr + f.size());
      alloc_size -= f.size();
    }

  if (alloc_size)
    panic("Kmem_alloc: cannot allocate sufficient kernel memory (missing %ld)",
          alloc_size);

  unsigned long freemap_size = Alloc::free_map_bytes(min_virt, max_virt);
  Address min_addr_kern = min_virt;

  setup_kmem_from_kip_md_tmp(freemap_size, min_addr_kern);
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
  Address virt = Super_pg::trunc(reinterpret_cast<Address>(_kernel_image_start));
  Address size = Super_pg::round(reinterpret_cast<Address>(_initcall_end)) - virt;
  auto pte = Kmem::kdir->walk(Virt_addr(virt), Kpdir::super_level(), false,
                              Ptab::Null_alloc(), Identity_map());
  assert(pte.is_valid());
  unsigned long phys = pte.page_addr();
  Mem_layout::add_pmem(phys, virt, size);

  // The existing 1:1 mapping is *not* removed! In case of cpu_virt on arm32 it
  // is required to exist for Mem_op::arm_mem_cache_maint().
}

STATIC_INITIALIZER_P(add_initial_pmem, BOOTSTRAP_INIT_PRIO);

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu]:

#include "kmem.h"
#include "paging.h"

/**
 * Map desired region as kernel heap and initialize buddy allocator.
 */
PRIVATE void
Kmem_alloc::map_heap(unsigned long start, unsigned long end)
{
  auto touched =
    Kmem::kdir->add(start, end,
                    Mpu_region_attr::make_attr(L4_fpage::Rights::RW()),
                    false, Kpdir::Kernel_heap);
  if (!touched)
    panic("Error creating kernel heap region!\n");
  Mpu::sync(*Kmem::kdir, touched.value());
  Mem::isb();

  unsigned long freemap_size = Alloc::free_map_bytes(start, end);
  Address min_addr_kern = start;
  setup_kmem_from_kip_md_tmp(freemap_size, min_addr_kern);
}

/**
 * Allocate kernel heap and map it in the MPU.
 */
PRIVATE void
Kmem_alloc::init_alloc()
{
  Kip *kip = Kip::k();
  Free_regions_map map;
  unsigned long available_size = create_free_map(kip, &map);
  unsigned long alloc_size = determine_kmem_alloc_size(available_size);

  if (available_size < alloc_size)
    panic("Kmem_alloc: Not enough kernel memory available (%ld)\n",
          available_size);

  unsigned long start = ~0UL;
  unsigned long end;

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region &f = map[i];
      if (f.size() < alloc_size)
        continue;
      if (f.size() > alloc_size)
        f.start += (f.size() - alloc_size);

      for (auto k : Kip::all_instances())
        k->add_mem_region(Mem_desc(f.start, f.end,
                                   k == kip ? Mem_desc::Kernel_tmp
                                            : Mem_desc::Reserved,
                                   false, Mem_desc::Reserved_heap));
      start = f.start;
      end = f.end;
      break;
    }

  if (start == ~0UL)
    panic("Kmem_alloc: regions too small");

  map_heap(start, end);
}

/**
 * Map pre-reserved region as kernel heap.
 */
PRIVATE bool
Kmem_alloc::init_prealloc(Kip *k)
{
  for (auto &md: k->mem_descs_a())
    {
      if (md.type() == Mem_desc::Kernel_tmp)
        {
          unsigned long start = md.start();
          unsigned long end = md.end();
          map_heap(start, end);
          return true;
        }
    }

  return false;
}

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  Kip *k = Kip::k();
  if (!init_prealloc(k))
    init_alloc();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu && amp]:

#include "amp_node.h"

/**
 * Reserve kernel memory for additional AMP nodes.
 *
 * Just mark it reserved in the Kip and return the base address.
 *
 * \param node The logical AMP node-id for which the memory shall be reserved.
 */
PUBLIC static void
Kmem_alloc::reserve_amp_heap(unsigned node)
{
  unsigned phys_node = cxx::int_value<Amp_phys_id>(Amp_node::phys_id(node));
  Free_regions_map map;
  create_free_map(Kip::all_instances()[node], &map);
  unsigned long alloc_size = Kmem_alloc::orig_free();

  for (int i = map.length() - 1; i >= 0; --i)
    {
      Mem_region &f = map[i];
      if (f.size() < alloc_size)
        continue;

      f.start += (f.size() - alloc_size);

      for (auto k : Kip::all_instances())
        k->add_mem_region(Mem_desc(f.start, f.end,
                                   k->node == phys_node ? Mem_desc::Kernel_tmp
                                                        : Mem_desc::Reserved,
                                   false, Mem_desc::Reserved_heap));
      return;
    }

  panic("Cannot reserve kmem for node %u\n", node);
}
