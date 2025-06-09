INTERFACE [arm]:

#include "kip.h"

class Kip_init
{
public:
  static void init();
};


//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstring>

#include "amp_node.h"
#include "config.h"
#include "mem_layout.h"
#include "mem_space.h"
#include "mem_unit.h"
#include "timer.h"


// Make the stuff below appearing only in this compilation unit.
// Trick Preprocess to let the struct reside in the cc file rather
// than putting it into the _i.h file which is perfectly wrong in 
// this case.
namespace KIP_namespace
{
  // See also restrictions about KIP layout in the kernel linker script!
  enum
  {
    Num_mem_descs = 64,
    Max_len_version = 512,

    Size_mem_descs = sizeof(Mword) * 2 * Num_mem_descs,
  };

  template<unsigned NODE = 0>
  struct KIP
  {
    Kip kip = {
        /* 00 */ L4_KERNEL_INFO_MAGIC,
                 Config::Kernel_version_id,
                 (Size_mem_descs + sizeof(Kip)) >> 4,
                 {}, 0,
                 cxx::int_value<Amp_phys_id>(Amp_node::phys_id(NODE)),
                 {},
        /* 10 */ 0, 0,
        /* 20 */ 0, 0,
        /* 30 */ 0, 0,
        /* 40 */ 0, 0,
        /* 50 */ 0, sizeof(Kip), Num_mem_descs, {},
	/* 60 */ {0},
        /* 70 */ {"", 0, {0}},
      };
    char mem_descs[Size_mem_descs] = {0};
  };

  KIP my_kernel_info_page asm("my_kernel_info_page") __attribute__((section(".kernel_info_page")));
};

IMPLEMENT
void Kip_init::init()
{
  Kip *kinfo = Kip::all_instances()[Amp_node::id()];
  Kip::init_global_kip(kinfo);
  kinfo->add_mem_region(Mem_desc(0, Mem_space::user_max(),
                        Mem_desc::Conventional, true));
  init_syscalls(kinfo);
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

  *reinterpret_cast<Mword*>(k->b + 0x970) = Timer::get_scaler_shift_ts_to_us().scaler;
  *reinterpret_cast<Mword*>(k->b + 0x978) = Timer::get_scaler_shift_ts_to_us().shift;
  *reinterpret_cast<Mword*>(k->b + 0x9f0) = Timer::get_scaler_shift_ts_to_ns().scaler;
  *reinterpret_cast<Mword*>(k->b + 0x9f8) = Timer::get_scaler_shift_ts_to_ns().shift;

  memcpy(k->b + 0x900, kip_time_fn_read_us,
         kip_time_fn_read_us_end - kip_time_fn_read_us);
  memcpy(k->b + 0x980, kip_time_fn_read_ns,
         kip_time_fn_read_ns_end - kip_time_fn_read_ns);

  Mem_unit::make_coherent_to_pou(k->b + 0x900, 0x100);
}

//--------------------------------------------------------------
IMPLEMENTATION[64bit]:

PRIVATE static inline
void
Kip_init::init_syscalls(Kip *kinfo)
{
  union K
  {
    Kip k;
    Mword w[0x1000 / sizeof(Mword)];
  };
  K *k = reinterpret_cast<K *>(kinfo);
  k->w[0x800 / sizeof(Mword)] = 0xd65f03c0d4000001; // svc #0; ret
}

//--------------------------------------------------------------
IMPLEMENTATION[32bit]:

PRIVATE static inline
void
Kip_init::init_syscalls(Kip *)
{}

//--------------------------------------------------------------
IMPLEMENTATION[mpu]:

#include "kmem.h"

/**
 * Map KIP in separate region to make it accessible to user space.
 *
 * There is a dedicated KIP region in the MPU so that user space can get
 * access to it. So far the kernel region covered it. Remove it from there
 * and establish the dedicated region.
 */
PUBLIC static
void
Kip_init::map_kip(Kip *k)
{
  extern char _kernel_image_start;
  extern char _kernel_kip_end;

  auto diff = Kmem::kdir->del(reinterpret_cast<Mword>(&_kernel_image_start),
                              reinterpret_cast<Mword>(&_kernel_kip_end) - 1U);
  diff |= Kmem::kdir->add(reinterpret_cast<Mword>(k),
                          reinterpret_cast<Mword>(k) + 0xfffU,
                          Mpu_region_attr::make_attr(L4_fpage::Rights::URX()),
                          false, Kpdir::Kip);

  if (!diff)
    panic("Cannot map KIP\n");

  Mpu::sync(*Kmem::kdir, diff.value(), true);
}

//--------------------------------------------------------------
IMPLEMENTATION[!mpu]:

PUBLIC static inline
void
Kip_init::map_kip(Kip *)
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && amp]:

namespace KIP_namespace
{
  /**
   * KIP structures for additional AMP nodes.
   *
   * The primary KIP (my_kernel_info_page) is amended by the linker script with
   * the version and feature strings (see .initkip.version and
   * .initkip.features sections). It is finally expanded to a full page by an
   * alignment directive.
   *
   * In contrast, the additional KIPs are only partially initialized and lack
   * all these strings. They need to be amended by bootstrap before booting the
   * kernel. The size is explicitly expanded by the `page` array, though.
   */
  template<unsigned NODES>
  struct Amp_kip : Amp_kip<NODES - 1>
  {
    KIP<NODES> kip;
    char page[Config::PAGE_SIZE - sizeof(KIP<NODES>)] = {0};
  };

  template<>
  struct Amp_kip<0>
  {};

  // Add a dedicated KIP for each additional AMP core.
  Amp_kip<Amp_node::Max_num_nodes - 1>
  amp_kernel_info_pages __attribute__((used,section(".kernel_info_page.amp")));

  static_assert(sizeof(amp_kernel_info_pages)
                  == (Amp_node::Max_num_nodes - 1) * Config::PAGE_SIZE,
                "Additional KIPs must be PAGE_SIZE size each!");
};
