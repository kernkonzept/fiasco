IMPLEMENTATION [arm && !mmu]:

#include "boot_infos.h"
#include "paging.h"
#include "static_init.h"

static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_mpu_dta =
{};

static Kpdir kmpu INIT_PRIORITY(BOOTSTRAP_INIT_PRIO);
Kpdir *Mem_layout::kdir = &kmpu;

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu]:

#include "kip.h"
#include "panic.h"

extern char _kip_start[];
extern char _kernel_image_start[];
extern char _initcall_end[];

/**
 * Setup MPU in the kernel.
 *
 * The Fiasco MPU boot protocol allows to be called with enabled MPU. In
 * this case region 0 must be the one that covers (at least) the kernel
 * image.
 */
static void
setup_mpu()
{
  auto diff = kmpu.add((Mword)_kernel_image_start,
                       (Mword)_initcall_end - 1U,
                       Mpu_region_attr::make_attr(L4_fpage::Rights::RWX()),
                       false, Kpdir::Kernel_text);

  // Will probably be never seen because UART is not setup yet. :(
  if (diff & Mpu_regions::Error)
    panic("Error creating MPU regions!\n");

  // Initialize MPU with in-place updates of MPU regions. This is important
  // as the MPU might already be used if the platform default background
  // region is not suitable.
  Mpu::init();
  Mpu::sync(&kmpu, diff, true);
  if (!Mpu::enabled())
    Mmu<0, true>::inv_cache();
  Mpu::enable();
  Mem::isb();
}

STATIC_INITIALIZER_P(setup_mpu, BOOTSTRAP_INIT_PRIO);
