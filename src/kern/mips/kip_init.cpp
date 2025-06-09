INTERFACE [mips]:

#include "kip.h"

class Kip_init {};


//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

#include <cstring>

#include "config.h"
#include "mem_layout.h"
#include "mem_unit.h"


// Make the stuff below appearing only in this compilation unit.
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
	/* 70 */ {"", 0, 0, {}},
      },
      {}
    };
};

PUBLIC static
void Kip_init::init()
{
  Kip *kinfo = reinterpret_cast<Kip*>(&KIP_namespace::my_kernel_info_page);
  Kip::init_global_kip(kinfo);

  kinfo->add_mem_region(Mem_desc(0, Mem_layout::User_max,
                        Mem_desc::Conventional, true));
}

PUBLIC static
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

  memcpy(k->b + 0x900, kip_time_fn_read_us,
         kip_time_fn_read_us_end - kip_time_fn_read_us);
  memcpy(k->b + 0x980, kip_time_fn_read_ns,
         kip_time_fn_read_ns_end - kip_time_fn_read_ns);

  Mem_unit::make_coherent_to_pou(k->b + 0x900, 0x100);
}
