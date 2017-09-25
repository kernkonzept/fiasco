INTERFACE [ia32,ux,amd64]:

#include "initcalls.h"
#include "types.h"
#include "kip.h"

class Cpu;


class Kip_init
{
public:
  /**
   * Insert memory descriptor for the Kmem region and finish the memory
   * info field.
   * @post no more memory descriptors may be added
   */
  static void setup_kmem_region (Address kmem_base, Address kmem_size);
};

IMPLEMENTATION [!ux]:

PRIVATE static inline
void
Kip_init::setup_ux(Kip *)
{}

IMPLEMENTATION [ia32,ux,amd64]:

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
  enum
  {
    Num_mem_descs = 50,
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
	/* 00/00  */ L4_KERNEL_INFO_MAGIC,
	             Config::Kernel_version_id,
	             (Size_mem_descs + sizeof(Kip)) >> 4,
	             {}, 0, {},
	/* 10/20  */ 0, {},
	/* 20/40  */ 0, 0, {},
	/* 30/60  */ 0, 0, {},
	/* 40/80  */ 0, 0, {},
	/* 50/A0  */ 0, (sizeof(Kip) << (sizeof(Mword)*4)) | Num_mem_descs, {},
	/* 60/C0  */ {},
	/* A0/140 */ 0, 0, 0, 0,
	/* B0/160 */ {},
	/* E0/1C0 */ 0, 0, {},
	/* F0/1D0 */ {"", 0}, {},
      },
      {}
    };
};

PUBLIC static FIASCO_INIT
//IMPLEMENT
void Kip_init::init()
{
  Kip *kinfo = reinterpret_cast<Kip*>(&KIP_namespace::my_kernel_info_page);
  setup_ux(kinfo);

  Kip::init_global_kip(kinfo);

  Kip::k()->clock = 0;
  Kip::k()->sched_granularity = Config::Scheduler_granularity;

  setup_user_virtual(kinfo);

  reserve_amd64_hole();


  extern char _boot_sys_start[];
  extern char _boot_sys_end[];

  for (auto &md: kinfo->mem_descs_a())
    {
      if (md.type() != Mem_desc::Reserved || md.is_virtual())
	continue;

      if (md.start() == (Address)_boot_sys_start
	  && md.end() == (Address)_boot_sys_end - 1)
	md.type(Mem_desc::Undefined);

      if (md.contains(Kmem::kernel_image_start())
	  && md.contains(Kmem::kcode_end()-1))
	{
	  md = Mem_desc(Kmem::kernel_image_start(), Kmem::kcode_end() -1,
	                Mem_desc::Reserved);
	}
    }
}


IMPLEMENTATION [amd64]:

PRIVATE static inline NOEXPORT NEEDS["kip.h"]
void
Kip_init::reserve_amd64_hole()
{
  enum { Trigger = 0x0000800000000000UL };
  Kip::k()->add_mem_region(Mem_desc(Trigger, ~Trigger, 
	                   Mem_desc::Reserved, true));
}

IMPLEMENTATION [!amd64]:

PRIVATE static inline NOEXPORT
void
Kip_init::reserve_amd64_hole()
{}

IMPLEMENTATION [!ux]:

PUBLIC static FIASCO_INIT
void
Kip_init::setup_user_virtual(Kip *kinfo)
{
  kinfo->add_mem_region(Mem_desc(0, Mem_layout::User_max,
                        Mem_desc::Conventional, true));
}
