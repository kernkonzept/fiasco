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
IMPLEMENTATION [arm_v8 && arm_lpae && !cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current()
{
  asm volatile (
      "msr TTBR0_EL1, %0            \n" // set TTBR
      "isb                          \n"
      :
      : "r" (Phys_mem_addr::val(_dir_phys) | (asid() << 48)));
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && arm_lpae && cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current()
{
  asm volatile (
      "msr VTTBR_EL2, %0            \n" // set TTBR
      "isb                          \n"
      :
      : "r" (Phys_mem_addr::val(_dir_phys) | (asid() << 48)));

  _current.current() = this;
}

