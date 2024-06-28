INTERFACE [amp]:

#include "amp_node.h"
#include "config.h"

class Kip;

EXTENSION class Platform_control
{
public:
  // Attention: constants are used in assembly!
  enum class Cpu_boot_info : unsigned char
  {
    Dead    = 0,
    Alive   = 1,
    Booting = 2,
    Booted  = 3,
  };

  static void __amp_entry() asm("__amp_entry");
  static void amp_ap_early_init();
  static unsigned amp_boot_info_idx() asm("amp_boot_info_idx");

  static Cpu_boot_info boot_info[Amp_node::Max_num_nodes - 1] asm("amp_boot_info");
};

// ------------------------------------------------------------------------
IMPLEMENTATION [amp]:

#include "amp_node.h"
#include "kip.h"
#include "kmem_mmio.h"
#include "kmem_alloc.h"
#include "koptions.h"
#include "mem_unit.h"
#include "minmax.h"
#include "mmio_register_block.h"
#include "poll_timeout_counter.h"
#include "warn.h"

__attribute__((section(".init.data")))
Platform_control::Cpu_boot_info Platform_control::boot_info[Amp_node::Max_num_nodes - 1];

/**
 * Get index into `boot_info` for early AMP startup code.
 *
 * Will be called without stack from __amp_entry!
 */
IMPLEMENT unsigned __attribute__((__flatten__))
Platform_control::amp_boot_info_idx()
{
  return Amp_node::id() - 1;
}

PUBLIC static void
Platform_control::amp_ap_init()
{
  Mword id = amp_boot_info_idx();
  boot_info[id] = Cpu_boot_info::Booted;
  Mem_unit::clean_dcache(&boot_info[id]);
  asm volatile ("sev");
}

IMPLEMENT_DEFAULT inline
void Platform_control::amp_ap_early_init()
{}

/**
 * Boot other AMP CPUs.
 *
 * It is assumed that the early BSP initialization called amp_prepare_ap_cpus()
 * to redirect them to the AMP trampoline code. The trampoline will announce
 * the presence of the CPU in the boot_info array.
 *
 * \pre Called on the first AMP node.
 */
PROTECTED static void
Platform_control::amp_boot_ap_cpus(int num)
{
  // Only the boot CPU might release the others from their spin loop
  assert(Amp_node::phys_id() == Amp_node::first_node());

  num = min<int>(num, Amp_node::Max_num_nodes);

  // Reserve memory for all AP CPUs up-front so that everybody sees the same
  // reserved regions.
  for (int i = 1; i < num; i++)
    Kmem_alloc::reserve_amp_heap(i);

  auto flush_dcache = [](Cpu_boot_info *info)
    { Mem_unit::flush_dcache(info, offset_cast<void *>(info, sizeof(*info))); };

  // Boot AP CPUs. Do it serially because they share the same initial stack.
  for (int i = 0; i < num - 1; i++)
    {
      Cpu_boot_info &info = boot_info[i];
      flush_dcache(&info);
      if (info != Cpu_boot_info::Alive)
        {
          WARN("CPU[%d] not alive!\n", i + 1);
          continue;
        }

      info = Cpu_boot_info::Booting;
      flush_dcache(&info);
      asm volatile ("sev");

      L4::Poll_timeout_counter guard(0x100000);
      do
        {
          Proc::pause();
          flush_dcache(&info);
        }
      while (guard.test(info != Cpu_boot_info::Booted));

      if (guard.timed_out())
        WARNX(Error, "CPU[%d] did not show up!\n", i + 1);
    }
}

/**
 * Redirect AP CPUs to out trampoline code.
 *
 * Should be done as early as possible to give the other CPUs a chance to
 * raise their "alive" flag.
 */
PUBLIC static void
Platform_control::amp_prepare_ap_cpus()
{
  Address spin_addr = Koptions::o()->core_spin_addr;
  if (spin_addr == 0 || spin_addr == static_cast<Address>(-1))
    return;

  // Only the boot CPU must issue the redirection
  if (Amp_node::phys_id() != Amp_node::first_node())
    return;

  // Redirect the other cores to our __amp_entry trampoline
  void *mmio = Kmem_mmio::map(spin_addr, sizeof(Address));
  Mmio_register_block s(mmio);
  s.r<Address>(0) = reinterpret_cast<Address>(&Platform_control::__amp_entry);
  Mem::dsb();
  Mem_unit::clean_dcache(s.get_mmio_base());
  asm volatile ("sev");
  Kmem_mmio::unmap(mmio, sizeof(Address));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [amp && 32bit]:

asm (
  ".text                          \n"
  ".balign 32                     \n" // Make sure it's usable as (H)VBAR
  ".globl __amp_entry             \n"
  "__amp_entry:                   \n"
  "  ldr sp, =_stack              \n"
  "  ldr r4, =amp_boot_info       \n"
  "                               \n"
  "  bl amp_boot_info_idx         \n" // get index into boot_info
  "  add r4, r4, r0               \n"
  "                               \n"
  "  mov r0, #1                   \n" // set "alive" flag
  "  strb r0, [r4]                \n"
  "  dsb                          \n"
  "                               \n"
  "1:ldrb r0, [r4]                \n" // wait for "booting" state
  "  cmp r0, #2                   \n" // Cpu_boot_info::Booting
  "  beq __amp_main               \n"
  "  wfe                          \n"
  "  dsb                          \n"
  "  b 1b                         \n"
);
