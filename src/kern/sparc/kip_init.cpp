INTERFACE [sparc]:

#include "kip.h"

class Kip_init
{
public:
  static void init();
};


//---------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include "mem_layout.h"
#include "config.h"

// Make the stuff below apearing only in this compilation unit.
// Trick Preprocess to let the struct reside in the cc file rather
// than putting it into the _i.h file which is perfectly wrong in 
// this case.
namespace KIP_namespace
{
  // See also restrictions about KIP layout in the kernel linker script!
  enum
  {
    Num_mem_descs = 20,
    Max_len_version = 512,

    Size_mem_descs = sizeof(Mword) * 2 * Num_mem_descs,
  };

  struct KIP
  {
    Kip kip;
    char mem_descs[Size_mem_descs];
  };

  KIP sparc_kernel_info_page asm("sparc_kernel_info_page") __attribute__((section(".kernel_info_page"))) =
    {
      {
	/* 00/00  */ L4_KERNEL_INFO_MAGIC,
	             Config::Kernel_version_id,
	             (Size_mem_descs + sizeof(Kip)) >> 4,
	             {}, 0, 0, {},
	/* 10/20  */ 0, {},
	/* 20/40  */ 0, 0, {},
	/* 30/60  */ 0, 0, {},
	/* 40/80  */ 0, 0, {},
	/* 50/A0  */ 0, (sizeof(Kip) << (sizeof(Mword)*4)) | Num_mem_descs, {},
	/* 60/C0  */ {},
	/* A0/140 */ 0, 0, 0, 0,
	/* B8/160 */ 0, {},
	/* E0/1C0 */ 0, 0, {},
	/* F0/1E0 */ {"", 0}, {0},
      },
      {}
    };
};


IMPLEMENT
void Kip_init::init()
{
  Kip *kinfo = reinterpret_cast<Kip*>(&KIP_namespace::sparc_kernel_info_page);
  Kip::init_global_kip(kinfo);

  /* add kernel image */
  kinfo->add_mem_region(Mem_desc(0,(Address)&Mem_layout::end - 1,
                        Mem_desc::Reserved));

  kinfo->add_mem_region(Mem_desc(0, Mem_layout::User_max,
	                Mem_desc::Conventional, true));
}
