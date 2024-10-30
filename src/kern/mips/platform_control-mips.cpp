INTERFACE:

#include "types.h"

EXTENSION class Platform_control
{
public:
  static void boot_all_secondary_cpus();
};

// ------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

IMPLEMENT_DEFAULT inline
void
Platform_control::boot_all_secondary_cpus()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include <cstdio>

#include "cm.h"
#include "kmem_alloc.h"
#include "mem_unit.h"

static void *_bev;

IMPLEMENT_DEFAULT inline
void
Platform_control::boot_all_secondary_cpus()
{
  boot_all_secondary_cpus_cm();
}

PRIVATE static inline NOEXPORT
Unsigned32 *
Platform_control::copy_code(Unsigned32 *dst, void const *s, void const *e)
{
  unsigned long sz =
    static_cast<char const *>(e) - static_cast<char const *>(s);
  memcpy(dst, s, sz);
  return dst + (sz / 4);
}

PRIVATE
static void *
Platform_control::alloc_secondary_boot_code()
{
  if (_bev)
    return _bev;

#define COPY_MP_INIT(pos, name)        \
  ({                                   \
   extern char name[];                 \
   extern char name ## _end[];         \
   copy_code(pos, name, name ## _end); \
   })

  // allocate 4KB as we need a 4KB alignemnt for the boot vector
  _bev = Kmem_alloc::allocator()->alloc(Bytes(4096));
  Unsigned32 *p = static_cast<Unsigned32 *>(_bev);

  unsigned cca = Mips::Cfg<0>::read().k0();

  switch (cca)
    {
    case 4: // CWBE coherent exclusive write back
    case 5: // CWB coherent write back
      break;
    default:
      printf("MIPS: Error, non-coherent CCA on boot CPU, disabling SMP.\n");
      return nullptr;
    }

  p = COPY_MP_INIT(p, _tramp_mp_e0);

  // CCA -> $v0 for segctl and cache init
  *(p++) = 0x34020000 | cca; // ORI $2, $0, cca

  if (Cpu::options.segctl())
    p = COPY_MP_INIT(p, _tramp_mp_segctl);

  if (Cm::present())
    {
      // this snippet enables coherency
      // GCR base -> $v1
      uintptr_t base = reinterpret_cast<uintptr_t>(Cm::cm->mmio_base());

      *(p++) = 0x3c030000 | static_cast<Unsigned32>(base >> 16); // LUI $3, ...
      *(p++) = 0x34630000 | (base & 0xffff); // ORI $3, $3, ...
      *(p++) = 0x8c642008; // lw $a0, 0x2008($v1) -> read coh_en in CM to $a0,
                           // used for chache init und cm init below
    }
  else
    *(p++) = 0x24040000; // li $a0, 0

  // needs $a0 == 0 if the cache needs initialization
  p = COPY_MP_INIT(p, _tramp_mp_cache);

  if (Cm::present())
    {
      if (Cm::cm->revision() < Cm::Rev_cm3)
        p = COPY_MP_INIT(p, _tramp_mp_cm);
      else
        p = COPY_MP_INIT(p, _tramp_mp_cm3);
    }

  p = COPY_MP_INIT(p, _tramp_mp_jmp_entry);

  Mem_unit::dcache_flush(reinterpret_cast<Address>(_bev),
                         reinterpret_cast<Address>(p));

#undef COPY_MP_INIT
  return _bev;
}

PRIVATE
static void
Platform_control::boot_all_secondary_cpus_cm()
{
  if (!Cm::present())
    return;

  if (!Cm::cm->cpc_present())
    return;

  void const *bev = alloc_secondary_boot_code();
  if (!bev)
    return;

  Address e = Mem_layout::pmem_to_phys(bev) | Mem_layout::KSEG1;
  Cm::cm->start_all_vps(e);
}

