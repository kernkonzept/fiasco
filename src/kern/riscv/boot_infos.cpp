INTERFACE [riscv]:

#include "types.h"
#include "mem_layout.h"
#include "paging_bits.h"

#define FIASCO_BOOT_PAGING_INFO \
  __attribute__((section(".bootstrap.info"), used))


/**
 * Boot_paging_info is used to coordinate the paging bootstrap mechanism with
 * the subsequent Kmem initialization.
 */
class Boot_paging_info
{
public:
  constexpr Boot_paging_info()
  {};

  constexpr Boot_paging_info(void *page_memory, unsigned num_pages)
  : _page_memory(page_memory),
    // The free map is initialized with only num_pages - 1,
    // as the kernel root page table is preallocated.
    _free_map((1 << (num_pages - 1)) - 1)
  {}

  Virt_addr kernel_page_directory()
  {
    // First page is preallocated for the kernel root page table.
    return Virt_addr(page_memory(0));
  }

  struct Boot_mem_map
  {
    static Address phys_to_pmem(Address addr)
    { return addr; }
  };

  struct Boot_alloc
  {
    typedef Address (*To_phys)(Virt_addr);

    Boot_alloc(Address base, Mword &free_map, To_phys to_phys)
    : _base(base),
      _free_map(free_map),
      _to_phys(to_phys)
    {}

    Address to_phys(void *virt) const
    { return _to_phys ? _to_phys(virt) : reinterpret_cast<Address>(virt); }

    bool valid() const { return true; }

    void *alloc(Bytes size)
    {
      (void) size;

      int x = __builtin_ffsl(_free_map);
      if (x == 0)
        return nullptr; // OOM

      _free_map &= ~(1UL << (x - 1));

      return reinterpret_cast<void *>(_base + Pg::size(x - 1));
    }

    Address _base;
    Mword &_free_map;
    To_phys _to_phys;
  };

  Boot_mem_map mem_map() const
  {
    return Boot_mem_map();
  }

  Boot_alloc alloc_phys(Boot_alloc::To_phys to_phys)
  {
    // Boot_alloc returns physical addresses, Boot_alloc::to_phys()
    // is an identity mapping.
    return Boot_alloc(to_phys(Virt_addr(page_memory(1))), _free_map, nullptr);
  }

  Boot_alloc alloc_virt(Boot_alloc::To_phys to_phys)
  {
    // Boot_alloc returns virtual addresses.
    return Boot_alloc(page_memory(1), _free_map, to_phys);
  }

  void kernel_image_size(Mword kernel_image_size)
  { _kernel_image_size = kernel_image_size; }

  Mword kernel_image_size() const
  { return _kernel_image_size; }

private:
  Address page_memory(unsigned page_num) const
  {
      return   Pg::round(reinterpret_cast<Address>(_page_memory))
             + Pg::size(page_num);
  }

  void *_page_memory = nullptr;
  Mword _free_map = 0;
  Mword _kernel_image_size = 0;
};
