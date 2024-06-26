INTERFACE [arm && 64bit]:

#define FIASCO_BOOT_PAGING_INFO \
  __attribute__((section(".bootstrap.info"), used))

// ------------------------------------------------------------------------
INTERFACE [arm && 64bit && mmu && !cpu_virt]:

struct Boot_paging_info
{
  void *l0_dir;
  void *l0_vdir;
  void *scratch;
  Mword free_map;
};

// ------------------------------------------------------------------------
INTERFACE [arm && 64bit && mmu && cpu_virt]:

struct Boot_paging_info
{
  void *l0_dir;
  void *scratch;
  Mword free_map;
};

// ------------------------------------------------------------------------
INTERFACE [arm && 64bit && !mmu]:

struct Boot_paging_info
{
  // Build fails if structure is empty...
  void *dummy;
};
