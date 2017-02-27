INTERFACE [arm && 64bit]:

#define FIASCO_BOOT_PAGING_INFO \
  __attribute__((section(".bootstrap.info"), used))

// ------------------------------------------------------------------------
INTERFACE [arm && 64bit && !cpu_virt]:

struct Boot_paging_info
{
  void *l0_dir;
  void *l1_dir;
  void *l0_vdir;
  void *l1_vdir;
  void *l2_mmio_dir;
};

// ------------------------------------------------------------------------
INTERFACE [arm && 64bit && cpu_virt]:

struct Boot_paging_info
{
  void *l0_dir;
  void *l1_dir;
  void *l1_vdir;
  void *l2_mmio_dir;
};
