INTERFACE:

#include "l4_types.h"
#include "config.h"

class Mem_layout
{
public:
  /// reflect symbols in linker script
  static const char load             asm ("_load");
  static const char image_start      asm ("_kernel_image_start");
  static const char start            asm ("_start");
  static const char end              asm ("_end");
  static const char ecode            asm ("_ecode");
  static const char etext            asm ("_etext");
  static const char data_start       asm ("_kernel_data_start");
  static const char edata            asm ("_edata");
  static const char initcall_start[] asm ("_initcall_start");
  static const char initcall_end[]   asm ("_initcall_end");

  /**
   * Return the number of bytes between initcall_start (inclusive) and
   * initcall_end(exclusive).
   */
  static size_t initcall_size()
  { return static_cast<size_t>(initcall_end - initcall_start); }

  /**
   * Translate physical address located in pmem to virtual address.
   *
   * \param phys  Physical address located in pmem.
   *              This address does not need to be page-aligned.
   *
   * \return Virtual address corresponding to phys.
   */
  static Address phys_to_pmem(Address phys);

  /**
   * Translate virtual address located in pmem to physical address.
   *
   * \param virt  Virtual address located in pmem.
   *              This address does not need to be page-aligned.
   *
   * \return Physical address corresponding to virt.
   */
  static Address pmem_to_phys(Address virt);

  static inline Address pmem_to_phys(const void *ptr)
  { return pmem_to_phys(reinterpret_cast<Address>(ptr)); }

  static bool in_kernel (Address a);

};


IMPLEMENTATION:

#include "config.h"

IMPLEMENT inline
bool
Mem_layout::in_kernel(Address a)
{
  return a > user_max();
}

PUBLIC static inline ALWAYS_INLINE
bool
Mem_layout::in_kernel_code (Address a)
{
  return a >= reinterpret_cast<Address>(&start) && a < reinterpret_cast<Address>(&ecode);
}
