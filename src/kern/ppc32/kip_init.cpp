INTERFACE [ppc32]:

#include "kip.h"

class Kip_init
{
public:
  static void init();
};


//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include "config.h"
#include "mem_layout.h"

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
        /* 70 */ {"", 0}, {0},
      },
      {}
    };
};


IMPLEMENT
void Kip_init::init()
{
  Kip *kinfo = reinterpret_cast<Kip*>(&KIP_namespace::my_kernel_info_page);
  Kip::init_global_kip(kinfo);

  /* add kernel image */
  kinfo->add_mem_region(Mem_desc(0,(Address)&Mem_layout::end - 1,
                        Mem_desc::Reserved));

  kinfo->add_mem_region(Mem_desc(0, Mem_layout::User_max,
	                Mem_desc::Conventional, true));
}
