IMPLEMENTATION [arm && !cpu_virt]:

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

PUBLIC inline NEEDS [Mem_space::virt_to_phys]
Address
Mem_space::pmem_to_phys(Address virt) const
{
  return virt_to_phys(virt);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt]:

static Address __mem_space_syscall_page;

IMPLEMENT_OVERRIDE inline
template< typename T >
T
Mem_space::peek_user(T const *addr)
{
  Address pa = virt_to_phys((Address)addr);
  if (pa == ~0UL)
    return ~0;

  Address ka = Mem_layout::phys_to_pmem(pa);
  if (ka == ~0UL)
    return ~0;

  return *reinterpret_cast<T const *>(ka);
}

PROTECTED static
void
Mem_space::set_syscall_page(void *p)
{
  __mem_space_syscall_page = (Address)p;
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

  Phys_mem_addr pa((Address)kern_lib_start - Mem_layout::Map_base
                   + Mem_layout::Sdram_phys_base);
  pte.set_page(pte.make_page(pa, Page::Attr(Page::Rights::URX(),
                                            Page::Type::Normal(),
                                            Page::Kern::Global())));

  pte.write_back_if(true, Mem_unit::Asid_kernel);

  pte = _dir->walk(Virt_addr(Mem_layout::Syscalls),
      Pdir::Depth, true, Kmem_alloc::q_allocator(ram_quota()));

  if (pte.level < Pdir::Depth - 1)
    return -1;

  pa = Phys_mem_addr(__mem_space_syscall_page);
  pte.set_page(pte.make_page(pa, Page::Attr(Page::Rights::URX(),
                                            Page::Type::Normal(),
                                            Page::Kern::Global())));

  pte.write_back_if(true, Mem_unit::Asid_kernel);

  return 0;
}

PUBLIC inline NEEDS [Mem_space::virt_to_phys, "kmem.h"]
Address
Mem_space::pmem_to_phys(Address virt) const
{
  return Kmem::kdir->virt_to_phys(virt);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm_v6]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void Mem_space::make_current()
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "mcr p15, 0, r0, c7, c10, 4   \n" // dsb
      "mcr p15, 0, %0, c2, c0       \n" // set TTBR
      "mcr p15, 0, r0, c7, c10, 4   \n" // dsb
      "mcr p15, 0, %1, c13, c0, 1   \n" // set new ASID value
      "mcr p15, 0, r0, c7, c5, 4    \n" // isb
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "mrc p15, 0, r1, c2, c0       \n"
      "mov r1, r1                   \n"
      "sub pc, pc, #4               \n"
      :
      : "r" (Phys_mem_addr::val(_dir_phys) | Page::Ttbr_bits), "r"(asid()), "r" (0)
      : "r1");
  _current.current() = this;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v7 || arm_v8) && !arm_lpae]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current()
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "dsb                          \n"
      "mcr p15, 0, %2, c13, c0, 1   \n" // change ASID to 0
      "isb                          \n"
      "mcr p15, 0, %0, c2, c0       \n" // set TTBR
      "isb                          \n"
      "mcr p15, 0, %1, c13, c0, 1   \n" // set new ASID value
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mov r1, r1                   \n"
      "sub pc, pc, #4               \n"
      :
      : "r" (Phys_mem_addr::val(_dir_phys) | Page::Ttbr_bits), "r"(asid()), "r" (0)
      : "r1");
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v7 || arm_v8) && arm_lpae && !cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current()
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mcrr p15, 0, %0, %1, c2      \n" // set TTBR
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mov r1, r1                   \n"
      "sub pc, pc, #4               \n"
      :
      : "r" (Phys_mem_addr::val(_dir_phys)), "r"(asid() << 16), "r" (0)
      : "r1");
  _current.current() = this;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(arm_v7 || arm_v8) && arm_lpae && cpu_virt]:

IMPLEMENT inline NEEDS[Mem_space::asid]
void
Mem_space::make_current()
{
// FIXME: flush bt only when reassigning ASIDs not on switch !!!!!!!
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mcrr p15, 6, %0, %1, c2      \n" // set TTBR
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mov r1, r1                   \n"
      "sub pc, pc, #4               \n"
      :
      : "r" (Phys_mem_addr::val(_dir_phys)), "r"(asid() << 16), "r" (0)
      : "r1");

  _current.current() = this;
}
