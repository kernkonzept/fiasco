INTERFACE:

#include "types.h"

class Kmem_mmio
{
public:
  static void *map(Address phys, size_t size, bool cache = false,
                   bool exec = false, bool global = true, bool buffered = false);
  static void unmap(void *ptr, size_t size);
};

INTERFACE[mmu]:

#include "global_data.h"
#include "spin_lock.h"
#include "page.h"
#include "paging.h"

EXTENSION class Kmem_mmio
{
private:
  static constexpr uintptr_t invalid_ptr = ~static_cast<uintptr_t>(0);

  static Global_data<Spin_lock<>> lock;
  static Global_data<uintptr_t> current;

  static Kpdir::Pte_ptr boot_kdir_walk(Virt_addr virt, unsigned level);
  [[nodiscard]] static bool boot_kdir_map(Address phys, Virt_addr virt,
                                          Virt_size size, Page::Attr attr,
                                          unsigned level);

  static uintptr_t find_unmapped_extent(size_t size_adj, uintptr_t start,
                                        uintptr_t end, size_t step,
                                        unsigned level);
  static uintptr_t find_mapped_extent(Address phys_adj, size_t size_adj,
                                      uintptr_t start, uintptr_t end,
                                      Page::Attr attr, size_t step,
                                      unsigned level);
  static bool map_extent(Address phys_adj, uintptr_t virt, size_t size_adj,
                         Page::Attr attr, unsigned level);
};

IMPLEMENTATION[mmu]:

#include "config.h"
#include "mem_layout.h"
#include "paging_bits.h"
#include "lock_guard.h"
#include "mem_unit.h"
#include "kmem.h"
#include "kmem_alloc.h"

DEFINE_GLOBAL Global_data<Spin_lock<>> Kmem_mmio::lock;
DEFINE_GLOBAL Global_data<uintptr_t>
  Kmem_mmio::current(Mem_layout::Mmio_map_start);

/**
 * Walk boot kernel paging directory.
 *
 * This is a default implementation that does nothing. If a target platform
 * supports the boot kernel paging directory, it has to override this method.
 *
 * \return Invalid page table entry.
 */
IMPLEMENT_DEFAULT
Kpdir::Pte_ptr
Kmem_mmio::boot_kdir_walk(Virt_addr, unsigned)
{ return Kpdir::Pte_ptr(); }

/**
 * Map physical memory into the boot kernel paging directory.
 *
 * This is a default implementation that does nothing. If a target platform
 * supports the boot kernel paging directory, it has to override this method.
 *
 * \retval false  Indicating failed mapping.
 */
IMPLEMENT_DEFAULT
bool
Kmem_mmio::boot_kdir_map(Address, Virt_addr, Virt_size, Page::Attr, unsigned)
{ return false; }

/**
 * Find an unmapped extent in the kernel virtual address space.
 *
 * Find an unmapped extent in the kernel virtual address space suitable for
 * the mapping of new physical memory. The method walks the #Kmem::kdir
 * directory if available or the boot kernel paging directory as a fallback.
 *
 * \note All method arguments are assumed to be already aligned to the
 *       respective (super)page size. It is also assumed that the (super)page
 *       size and (super)page level arguments are consistent.
 *
 * \param size_adj  Size of the extent to be mapped.
 * \param start     Starting virtual address to search.
 * \param end       Ending virtual address to search.
 * \param step      (Super)page size.
 * \param level     (Super)page level.
 *
 * \return Virtual address of the extent suitable for the mapping or
 *         #invalid_ptr if no suitable extent exists.
 */
IMPLEMENT_DEFAULT
uintptr_t
Kmem_mmio::find_unmapped_extent(size_t size_adj, uintptr_t start, uintptr_t end,
                                size_t step, unsigned level)
{
  size_t size_cur = 0;
  uintptr_t candidate = invalid_ptr;

  for (uintptr_t virt = start; virt < end; virt += step)
    {
      auto pte = Kmem::kdir
        ? Kmem::kdir->walk(Virt_addr(virt), level)
        : boot_kdir_walk(Virt_addr(virt), level);

      if (pte.is_valid())
        {
          // Page already mapped, reset search.
          size_cur = 0;
          candidate = invalid_ptr;
          continue;
        }

      // Page unmapped, track current extent as candidate.
      if (size_cur == 0)
        candidate = virt;

      size_cur += step;

      // Candidate extent is large enough.
      if (size_cur >= size_adj)
        return candidate;
    }

  return invalid_ptr;
}

/**
 * Find an already mapped extent in the kernel virtual address space.
 *
 * Find an already mapped extent in the kernel virtual address space that
 * covers the requested physical memory mapping. The method walks the
 * #Kmem::kdir directory if available or the boot kernel paging directory as
 * a fallback.
 *
 * The page attributes (rights, type, global, etc.) of the existing and
 * requested mapping are checked to be equal.
 *
 * \note All method arguments are assumed to be already aligned to the
 *       respective (super)page size. It is also assumed that the (super)page
 *       size and (super)page level arguments are consistent.
 *
 * \param phys_adj  Physical address of the extent to be mapped.
 * \param size_adj  Size of the extent to be mapped.
 * \param start     Starting virtual address to search.
 * \param end       Ending virtual address to search.
 * \param attr      Requested attributes of the mapping.
 * \param step      (Super)page size.
 * \param level     (Super)page level.
 *
 * \return Virtual address of the extent suitable for the mapping or
 *         #invalid_ptr if no suitable extent exists.
 */
IMPLEMENT_DEFAULT
uintptr_t
Kmem_mmio::find_mapped_extent(Address phys_adj, size_t size_adj,
                              uintptr_t start, uintptr_t end, Page::Attr attr,
                              size_t step, unsigned level)
{
  size_t size_cur = 0;
  uintptr_t candidate = invalid_ptr;

  for (uintptr_t virt = start; virt < end; virt += step)
    {
      auto pte = Kmem::kdir
        ? Kmem::kdir->walk(Virt_addr(virt), level)
        : boot_kdir_walk(Virt_addr(virt), level);

      if (!pte.is_valid() || pte.page_addr() != phys_adj + size_cur
          || pte.page_level() != level || !pte.attribs_compatible(attr))
        {
          // Page does not map the desired physical address or the attributes
          // of the mapping are incompatible. Reset search.
          size_cur = 0;
          candidate = invalid_ptr;
          continue;
        }

      // Page mapping found, track current extent as candidate.
      if (size_cur == 0)
        candidate = virt;

      size_cur += step;

      // Candidate extent is large enough.
      if (size_cur >= size_adj)
        return candidate;
    }

  return invalid_ptr;
}

/**
 * Map physical extent to kernel virtual memory.
 *
 * The method maps into the #Kmem::kdir directory if available or into the boot
 * kernel paging directory as a fallback.
 *
 * If the kernel memory allocator is not available, it is assumed that the
 * mapping can succeed without allocating new page tables. This usually
 * means that the mapping has to be done on a superpage level.
 *
 * \note All method arguments are assumed to be already aligned to the
 *       respective (super)page size consistent with the (super)page level.
 *
 * \param phys_adj  Physical address of the extent to map.
 * \param virt      Virtual address of the extent to map.
 * \param size_adj  Size of the extent to map.
 * \param attr      Requested attributes of the mapping.
 * \param level     (Super)page level of the requested mapping.
 *
 * \retval true   Mapping was successful.
 * \retval false  Mapping was not successful.
 */
IMPLEMENT_DEFAULT
bool
Kmem_mmio::map_extent(Address phys_adj, uintptr_t virt, size_t size_adj,
                      Page::Attr attr, unsigned level)
{
  if (Kmem::kdir)
    {
      if (Kmem_alloc::ready())
        return Kmem::kdir->map(phys_adj, Virt_addr(virt), Virt_size(size_adj),
                               attr, level, true,
                               pdir_alloc(Kmem_alloc::allocator()));
      else
        return Kmem::kdir->map(phys_adj, Virt_addr(virt), Virt_size(size_adj),
                               attr, level, true, Ptab::Null_alloc());
    }

  return boot_kdir_map(phys_adj, Virt_addr(virt), Virt_size(size_adj), attr,
                       level);
}

/**
 * Map physical memory to kernel virtual memory.
 *
 * This is usually used for mapping MMIO device memory, but it is suitable
 * also for other purposes. The page table entries are used directly for the
 * bookkeeping information.
 *
 * The method maps into the #Kmem::kdir directory if available or into the boot
 * kernel paging directory as a fallback. If the #Kmem::kdir directory is not
 * available or if the kernel memory allocator is not available, the mapping
 * is performed with superpage granularity, otherwise a regular page
 * granularity is used.
 *
 * \note If the mapping of a physical memory extent that is fully contained
 *       within an already mapped extent (with compatible attributes) is
 *       requested, the existing mapping is returned instead of establishing
 *       a new mapping.
 *
 * \param phys      Physical address of the extent to be mapped.
 * \param size      Size of the extent to be mapped.
 * \param cache     Map the extent as cached. The 'cache' parameter has
 *                  precedence over the 'buffered' parameter.
 *                  'cached' are mutually exclusive.
 * \param exec      Map the extent as executable.
 * \param global    Map the extent as global.
 * \param buffered  Map the extent as "buffered". The 'cache' parameter has
 *                  precedence over the 'buffered' parameter.
 *
 * \return Pointer to the mapped extent in kernel virtual memory or
 *         #nullptr if the mapping failed.
 */
IMPLEMENT_DEFAULT
void *
Kmem_mmio::map(Address phys, size_t size, bool cache, bool exec, bool global,
               bool buffered)
{
  auto guard = lock_guard(&lock);

  uintptr_t start;
  uintptr_t end;
  uintptr_t next;

  Address phys_adj;
  size_t offset;
  size_t size_adj;

  size_t step;
  unsigned level;

  if (Kmem::kdir && Kmem_alloc::ready())
    {
      start = Pg::round(Mem_layout::Mmio_map_start);
      end = Pg::trunc(Mem_layout::Mmio_map_end);
      next = Pg::round(current.unwrap());

      phys_adj = Pg::trunc(phys);
      offset = Pg::offset(phys);
      size_adj = Pg::round(offset + size);

      step = Config::PAGE_SIZE;
      level = Kpdir::Depth;
    }
  else
    {
      start = Super_pg::round(Mem_layout::Mmio_map_start);
      end = Super_pg::trunc(Mem_layout::Mmio_map_end);
      next = Super_pg::round(current.unwrap());

      phys_adj = Super_pg::trunc(phys);
      offset = Super_pg::offset(phys);
      size_adj = Super_pg::round(offset + size);

      step = Config::SUPERPAGE_SIZE;
      level = Kpdir::Super_level;
    }

  Page::Attr attr(exec ? Page::Rights::RWX() : Page::Rights::RW(),
                  cache ? Page::Type::Normal()
                        : buffered ? Page::Type::Buffered()
                                   : Page::Type::Uncached(),
                  global ? Page::Kern::Global() : Page::Kern::None(),
                  Page::Flags::None());

  // Search for existing mapping.
  uintptr_t virt = find_mapped_extent(phys_adj, size_adj, start, end, attr,
                                      step, level);
  if (virt != invalid_ptr)
    return reinterpret_cast<void *>(virt | offset);

  // Next fit approach.
  virt = find_unmapped_extent(size_adj, next, end, step, level);
  if (virt == invalid_ptr)
    {
      // First fit fallback.
      virt = find_unmapped_extent(size_adj, start, next, step, level);
      if (virt == invalid_ptr)
        return nullptr;
    }

  if (!map_extent(phys_adj, virt, size_adj, attr, level))
    return nullptr;

  for (size_t i = 0; i < size_adj; i += Config::PAGE_SIZE)
    Mem_unit::tlb_flush_kernel(virt + i);

  current = virt + size_adj;
  return reinterpret_cast<void *>(virt | offset);
}

/**
 * Unmap physical memory from kernel virtual memory.
 *
 * This method only unmaps from the #Kmem::kdir directory.
 *
 * \note It is assumed that the memory has been previously mapped using
 *       #Kmem_mmio::map. Otherwise the behavior is undefined.
 *
 * \param ptr   Pointer to the extent to be unmapped.
 * \param size  Size of the extent to be unmapped.
 */
IMPLEMENT_DEFAULT
void
Kmem_mmio::unmap(void *ptr, size_t size)
{
  auto guard = lock_guard(&lock);

  if (!Kmem::kdir)
    return;

  uintptr_t virt = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t virt_adj = Pg::trunc(virt);
  size_t size_adj = Pg::round(Pg::offset(virt) + size);

  if (virt_adj < Mem_layout::Mmio_map_start
      || virt_adj + size_adj > Mem_layout::Mmio_map_end)
    return;

  Kmem::kdir->unmap(Virt_addr(virt_adj), Virt_size(size_adj), Kpdir::Depth,
                    true);

  for (size_t i = 0; i < size_adj; i += Config::PAGE_SIZE)
    Mem_unit::tlb_flush_kernel(virt + i);
}
