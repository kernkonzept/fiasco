INTERFACE [amp]:

#include "config.h"

class Kip;

EXTENSION class Platform_control
{
public:
  struct Cpu_boot_info
  {
    Kip *kip;
    bool alive;
    bool booted;
  };

  static void __amp_entry() asm("__amp_entry");
  static void amp_ap_early_init();
  static unsigned amp_boot_info_idx() asm("amp_boot_info_idx");

  static Cpu_boot_info boot_info[Config::Max_num_nodes - 1] asm("amp_boot_info");
};

// ------------------------------------------------------------------------
IMPLEMENTATION [amp]:

#include "kmem_alloc.h"
#include "mem_unit.h"
#include <cstring>

__attribute__((section(".init.data")))
Platform_control::Cpu_boot_info Platform_control::boot_info[Config::Max_num_nodes - 1];

/**
 * Get node-id for early AMP startup code.
 *
 * Must be callable without stack!
 */
IMPLEMENT unsigned __attribute__((__flatten__))
Platform_control::amp_boot_info_idx()
{
  return Platform_control::node_id() - Config::First_node - 1;
}

PUBLIC static void
Platform_control::amp_ap_init()
{
  Mword id = amp_boot_info_idx();
  boot_info[id].booted = true;
  Mem_unit::clean_dcache(&boot_info[id].booted);
  asm volatile ("sev");
}

PRIVATE static Kip *
Platform_control::create_kip(unsigned long kmem, unsigned node)
{
  Kip *kip = (Kip *)Kmem_alloc::allocator()->alloc(Bytes(Config::PAGE_SIZE));
  if (!kip)
    panic("Cannot alloc new KIP\n");

  memcpy(kip, Kip::k(), Config::PAGE_SIZE);
  kip->node = node;
  kip->_mem_assign_base = kmem;
  kip->_mem_assign_len = Config::KMEM_SIZE;
  Mem_unit::flush_dcache(kip, (char *)kip + Config::PAGE_SIZE);

  return kip;
}

IMPLEMENT_DEFAULT inline
void Platform_control::amp_ap_early_init()
{}

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
  "  add r4, r4, r0, LSL #3       \n"
  "                               \n"
  "  mov r0, #1                   \n" // set "alive" flag
  "  strb r0, [r4, #4]            \n"
  "  dsb                          \n"
  "                               \n"
  "1:ldr r0, [r4]                 \n" // wait for our kip to be set
  "  cmp r0, #0                   \n"
  "  bne __amp_main               \n" // call __amp_main with kip pointer
  "  wfe                          \n"
  "  dsb                          \n"
  "  b 1b                         \n"
);
