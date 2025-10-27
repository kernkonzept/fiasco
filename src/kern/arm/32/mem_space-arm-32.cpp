IMPLEMENTATION [arm && !mmu]:

PROTECTED inline
int
Mem_space::sync_kernel()
{
  return 0;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && !cpu_virt]:

PROTECTED inline NEEDS["kmem_alloc.h"]
int
Mem_space::sync_kernel()
{
  return _dir->sync(Virt_addr(Mem_layout::User_max + 1), kernel_space()->_dir,
                    Virt_addr(Mem_layout::User_max + 1),
                    Virt_size(-(Mem_layout::User_max + 1)), Pdir::Super_level,
                    Pte_ptr::need_cache_write_back(this == _current.current()),
                    Kmem_alloc::q_allocator(_quota));
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && 32bit && cpu_virt]:

static Address __mem_space_syscall_page;

PROTECTED static
void
Mem_space::set_syscall_page(void *p)
{
  __mem_space_syscall_page =
    Mem_layout::pmem_to_phys(reinterpret_cast<Address>(p));
}

PROTECTED
int
Mem_space::sync_kernel()
{
  auto pte = _dir->walk(Virt_addr(Mem_layout::Kern_lib_base),
      Pdir::Depth, true, Kmem_alloc::q_allocator(ram_quota()));
  if (pte.level < Pdir::Depth - 1)
    return -1;

  extern char kern_lib_start[];

  pte.set_page(Kmem::kdir->virt_to_phys(kern_lib_start),
               Page::Attr::space_local(Page::Rights::URX()));

  pte.write_back_if(true, c_asid());

  pte = _dir->walk(Virt_addr(Mem_layout::Syscalls),
      Pdir::Depth, true, Kmem_alloc::q_allocator(ram_quota()));

  if (pte.level < Pdir::Depth - 1)
    return -1;

  pte.set_page(Phys_mem_addr(__mem_space_syscall_page),
               Page::Attr::space_local(Page::Rights::URX()));

  pte.write_back_if(true, c_asid());

  return 0;
}

IMPLEMENT_OVERRIDE inline NEEDS["mem_layout.h"]
Address
Mem_space::max_usable_user_address()
{ return Mem_layout::Kern_lib_base - 1; }

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm_v6 && mmu]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void Mem_space::make_current(Switchin_flags)
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "mcr p15, 0, r0, c7, c10, 4   \n" // CP15DSB
      "mcr p15, 0, %0, c2, c0       \n" // set TTBR0
      "mcr p15, 0, r0, c7, c10, 4   \n" // CP15DSB
      "mcr p15, 0, %1, c13, c0, 1   \n" // set new ASID value
      "mcr p15, 0, r0, c7, c5, 4    \n" // CP15ISB
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "mrc p15, 0, r1, c2, c0       \n"
      "mov r1, r1                   \n"
      "sub pc, pc, #4               \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys) | Page::Ttbr_bits), "r"(asid()), "r" (0)
      : "r1");
  _current.current() = this;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v7 || arm_v8) && mmu && !arm_lpae]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current(Switchin_flags)
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "dsb                          \n"
      "mcr p15, 0, %2, c13, c0, 1   \n" // change ASID to 0
      "isb                          \n"
      "mcr p15, 0, %0, c2, c0       \n" // set TTBR0
      "isb                          \n"
      "mcr p15, 0, %1, c13, c0, 1   \n" // set new ASID value
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys) | Page::Ttbr_bits), "r"(asid()), "r" (0)
      : "r1");
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v7 || arm_v8) && mmu && arm_lpae && !cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current(Switchin_flags)
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mcrr p15, 0, %0, %1, c2      \n" // set TTBR0
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys)), "r"(asid() << 16), "r" (0)
      : "r1");
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v7 || arm_v8) && mmu && arm_lpae && cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current(Switchin_flags)
{
// FIXME: flush bt only when reassigning ASIDs not on switch !!!!!!!
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mcrr p15, 6, %0, %1, c2      \n" // set VTTBR
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_dir_phys)), "r"(asid() << 16), "r" (0)
      : "r1");

  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm_v8 && mpu && !cpu_virt]:

#include "mpu.h"

IMPLEMENT
void
Mem_space::make_current(Switchin_flags)
{
  asm volatile (
      "mcr p15, 0, %0, c13, c0, 1"  // CONTEXTIDR - set new ASID value
      :
      : "r"(asid()));
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
  asm volatile (
      "mcr p15, 4, %0, c2, c0, 0" // VSCTLR - set new VMID value
      :
      : "r"(asid() << 16));
  _current.current() = this;

  auto guard = lock_guard(_lock);

  // No need for an isb here. This is done implicitly on exception return and
  // the kernel does not access these regions.
  Mpu::update(*_dir);
  mpu_state_mark_in_sync();
}
