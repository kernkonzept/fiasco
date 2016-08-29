IMPLEMENTATION [arm]:

typedef Unsigned64 K_ptab_array[512] __attribute__((aligned(0x1000)));

K_ptab_array kernel_l0_vdir;
K_ptab_array kernel_l1_vdir;
K_ptab_array kernel_l2_mmio_dir;
K_ptab_array kernel_l0_dir;
K_ptab_array kernel_l1_dir;
Kpdir *Kmem_space::_kdir = (Kpdir *)&kernel_l0_vdir;

// initialize the kernel space (page table)
IMPLEMENT inline void Kmem_space::init() {}

