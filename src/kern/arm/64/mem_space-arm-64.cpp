IMPLEMENTATION [arm]:

PROTECTED inline NEEDS["kmem_alloc.h"]
int
Mem_space::sync_kernel()
{
  // FIXME: add syscall page etc... if needed
  return 0;
}

PUBLIC inline NEEDS [Mem_space::virt_to_phys]
Address
Mem_space::pmem_to_phys(Address virt) const
{
  return virt - Mem_layout::Map_base + Mem_layout::Sdram_phys_base;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mmu && arm_lpae && !cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current()
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

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current()
{
  asm volatile (
      "msr VTTBR_EL2, %0            \n" // set TTBR
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys) | (asid() << 48)));

  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mpu && !cpu_virt]:

#include "mpu.h"
#include "context.h"

IMPLEMENT
void
Mem_space::make_current()
{
  current()->load_mpu_enable(this);
  Mpu::update(_dir);

  // No need for isb. This is done implicitly on the kernel exit which is a
  // "context synchronization event".

  asm volatile ("msr TTBR0_EL1, %0" : : "r" (asid() << 48));
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mpu && cpu_virt]:

#include "mpu.h"
#include "context.h"

IMPLEMENT
void
Mem_space::make_current()
{
  current()->load_mpu_enable(this);
  Mpu::update(_dir);

  // No need for isb. This is done implicitly on the kernel exit which is a
  // "context synchronization event".

  asm volatile ("msr S3_4_c2_c0_0, %0" : : "r" (asid() << 48)); // VSCTLR_EL2
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && !mmu && !mpu && cpu_virt]:

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && !mmu && !mpu && !cpu_virt]:

