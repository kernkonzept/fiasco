IMPLEMENTATION [arm && mpu]:

#include "cpu.h"
#include "kmem.h"
#include "mmu.h"
#include "paging.h"
#include "panic.h"
#include "static_init.h"

extern char _kernel_image_start[];
extern char _initcall_end[];

// Located here to be sure kmpu is constructed before setup_mpu() is called!
static Kpdir kmpu INIT_PRIORITY(BOOTSTRAP_INIT_PRIO);
Kpdir *Kmem::kdir = &kmpu;

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
  auto diff = kmpu.add(reinterpret_cast<Mword>(_kernel_image_start),
                       reinterpret_cast<Mword>(_initcall_end) - 1U,
                       Mpu_region_attr::make_attr(L4_fpage::Rights::RWX()),
                       false, Kpdir::Kernel_text);

  // Will probably be never seen because UART is not setup yet. :(
  if (!diff)
    panic("Error creating MPU regions!\n");

  // Initialize MPU with in-place updates of MPU regions. This is important
  // as the MPU might already be used if the platform default background
  // region is not suitable.
  Mpu::init();
  Mpu::sync(kmpu, diff.value(), true);
  if (!Mpu::enabled())
    Mmu<0, true>::inv_cache();
  Cpu::init_sctlr();
}

STATIC_INITIALIZER_P(setup_mpu, BOOTSTRAP_INIT_PRIO);
