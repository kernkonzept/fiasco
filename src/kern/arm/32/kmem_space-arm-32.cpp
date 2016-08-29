IMPLEMENTATION [arm]:

// always 16kB also for LPAE we use 4 consecutive second level tables
char kernel_page_directory[0x4000]
  __attribute__((aligned(0x4000), section(".bss.kernel_page_dir")));

// initialize the kernel space (page table)
IMPLEMENT
void Kmem_space::init()
{
  unsigned domains = 0x0001;

  asm volatile("mcr p15, 0, %0, c3, c0" : : "r" (domains));

  Mem_unit::clean_vdcache();
}

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_lpae]:

Unsigned64 kernel_lpae_dir[4] __attribute__((aligned(4 * sizeof(Unsigned64))));
Kpdir *Kmem_space::_kdir = (Kpdir *)&kernel_lpae_dir;

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && !arm_lpae]:

Kpdir *Kmem_space::_kdir = (Kpdir *)&kernel_page_directory;

