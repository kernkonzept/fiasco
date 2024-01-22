INTERFACE [arm]:

#include "paging.h"
#include "mem_layout.h"

class Kmem_space : public Mem_layout
{
public:
  static void init();
  static void init_hw();
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu]:

#include "boot_infos.h"

static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_mpu_dta =
{};
