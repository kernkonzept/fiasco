INTERFACE [ia32 || amd64]:

#include "initcalls.h"
#include "types.h"
#include "kip.h"

class Cpu;


class Kip_init
{
};

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include <cstring>
#include "config.h"
#include "cpu.h"
#include "div32.h"
#include "kmem.h"
#include "panic.h"


/** KIP initialization. */
PUBLIC static FIASCO_INIT
void
Kip_init::init_freq(Cpu const &cpu)
{
  Kip::k()->frequency_cpu	= div32(cpu.frequency(), 1000);
}


namespace KIP_namespace
{
  // See also restrictions about KIP layout in the kernel linker script!
  enum
  {
    Num_mem_descs = 64,
    Max_len_version = 512,

    Size_mem_descs = sizeof(Mword) * 2 * Num_mem_descs,
  };

  struct KIP
  {
    Kip kip;
    char mem_descs[Size_mem_descs];
  };

  KIP my_kernel_info_page asm("my_kernel_info_page") __attribute__((section(".kernel_info_page"))) =
    {
      {
        /* 00 */ L4_KERNEL_INFO_MAGIC,
                 Config::Kernel_version_id,
                 (Size_mem_descs + sizeof(Kip)) >> 4,
                 {}, 0,
                 0,
                 {},
        /* 10 */ 0, 0,
        /* 20 */ 0, 0,
        /* 30 */ 0, 0,
        /* 40 */ 0, 0,
        /* 50 */ 0, sizeof(Kip), Num_mem_descs, {},
        /* 60 */ {0},
        /* 70 */ {"", 0}, {},
      },
      {}
    };
};

PUBLIC static FIASCO_INIT
//IMPLEMENT
void Kip_init::init()
{
  Kip *kinfo = reinterpret_cast<Kip*>(&KIP_namespace::my_kernel_info_page);

  Kip::init_global_kip(kinfo);

  setup_user_virtual(kinfo);

  reserve_amd64_hole();


  extern char _boot_sys_start[];
  extern char _boot_sys_end[];

  for (auto &md: kinfo->mem_descs_a())
    {
      if (md.type() != Mem_desc::Reserved || md.is_virtual())
        continue;

      if (md.start() == reinterpret_cast<Address>(_boot_sys_start)
          && md.end() == reinterpret_cast<Address>(_boot_sys_end) - 1)
        md.type(Mem_desc::Undefined);

      if (md.contains(Kmem::kernel_image_start())
          && md.contains(Kmem::kcode_end() - 1))
        {
          md = Mem_desc(Kmem::kernel_image_start(), Kmem::kcode_end() - 1,
                        Mem_desc::Reserved);
        }
    }
}

PUBLIC static FIASCO_INIT
void
Kip_init::init_kip_clock()
{
  union K
  {
    Kip       k;
    Unsigned8 b[Config::PAGE_SIZE];
  };
  extern char kip_time_fn_read_us[];
  extern char kip_time_fn_read_us_end[];
  extern char kip_time_fn_read_ns[];
  extern char kip_time_fn_read_ns_end[];

  K *k = reinterpret_cast<K *>(Kip::k());

  Cpu cpu = Cpu::cpus.cpu(Cpu_number::boot_cpu());
  *reinterpret_cast<Mword*>(k->b + 0x9f0) = cpu.get_scaler_tsc_to_us();
  *reinterpret_cast<Mword*>(k->b + 0x9f8) = cpu.get_scaler_tsc_to_ns();

  memcpy(k->b + 0x900, kip_time_fn_read_us,
         kip_time_fn_read_us_end - kip_time_fn_read_us);
  memcpy(k->b + 0x980, kip_time_fn_read_ns,
         kip_time_fn_read_ns_end - kip_time_fn_read_ns);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

PRIVATE static inline NOEXPORT NEEDS["kip.h"]
void
Kip_init::reserve_amd64_hole()
{
  enum { Trigger = 0x0000800000000000UL };
  Kip::k()->add_mem_region(Mem_desc(Trigger, ~Trigger, 
	                   Mem_desc::Reserved, true));
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!amd64]:

PRIVATE static inline NOEXPORT
void
Kip_init::reserve_amd64_hole()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC static FIASCO_INIT
void
Kip_init::setup_user_virtual(Kip *kinfo)
{
  kinfo->add_mem_region(Mem_desc(0, Mem_layout::User_max,
                        Mem_desc::Conventional, true));
}
