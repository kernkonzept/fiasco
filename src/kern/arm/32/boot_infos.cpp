INTERFACE [arm && 32bit]:

#define FIASCO_BOOT_PAGING_INFO \
  __attribute__((section(".bootstrap.info"), used))

// --------------------------------------------------
INTERFACE [arm && 32bit && !arm_lpae]:

struct Boot_paging_info
{
  void *kernel_page_directory;
};

//---------------------------------------------------
INTERFACE [arm && 32bit && arm_lpae]:

struct Boot_paging_info
{
  void *kernel_page_directory;
  void *kernel_lpae_dir;
};
