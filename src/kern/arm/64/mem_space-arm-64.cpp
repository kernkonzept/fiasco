IMPLEMENTATION [arm]:

PROTECTED inline NEEDS["kmem_alloc.h"]
int
Mem_space::sync_kernel()
{
  // FIXME: add syscall page etc... if needed
  return 0;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mmu && arm_lpae && !cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current(Switchin_flags)
{
  asm volatile (
      "msr TTBR0_EL1, %0            \n" // set TTBR
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys) | (asid() << 48)));
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mmu && arm_lpae && cpu_virt]:

#include "cpu.h"
#include "paging.h"

EXTENSION class Mem_space
{
  struct Max_ipa_address
  {
    Max_ipa_address()
    : val((1ULL << Page::ipa_bits(Cpu::pa_range())) - 1U)
    {}

    Address val;
  };

  static Max_ipa_address _max_user_address;
};

Mem_space::Max_ipa_address
Mem_space::_max_user_address INIT_PRIORITY(EARLY_INIT_PRIO);

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current(Switchin_flags)
{
  asm volatile (
      "msr VTTBR_EL2, %0            \n" // set TTBR
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys) | (asid() << 48)));

  _current.current() = this;
}

/*
 * The page tables always provide up to Mem_layout::user_max() bits of virtual
 * address space. But at least on arm64 cpu_virt the HW supported stage1 output
 * size (maximum IPA size) is additionally constrained by the available physical
 * address size of the MMU.
 */
IMPLEMENT_OVERRIDE
Address
Mem_space::max_user_address()
{ return _max_user_address.val; }

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mpu && !cpu_virt]:

#include "mpu.h"

IMPLEMENT
void
Mem_space::make_current(Switchin_flags)
{
  asm volatile ("msr TTBR0_EL1, %0" : : "r" (asid() << 48));
  _current.current() = this;

  auto guard = lock_guard(_lock);

  // No need for an isb here. This is done implicitly on exception return and
  // the kernel does not access these regions.
  Mpu::update(*_dir);
  mpu_state_mark_in_sync();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mpu && cpu_virt]:

#include "mpu.h"

IMPLEMENT
void
Mem_space::make_current(Switchin_flags)
{
  asm volatile ("msr S3_4_c2_c0_0, %0" : : "r" (asid() << 48)); // VSCTLR_EL2
  _current.current() = this;

  auto guard = lock_guard(_lock);

  // No need for an isb here. This is done implicitly on exception return and
  // the kernel does not access these regions.
  Mpu::update(*_dir);
  mpu_state_mark_in_sync();
}
