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

#include "cm.h"
#include "cpc.h"
#include "kmem_alloc.h"

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
  unsigned long sz = (char const *)e - (char const *)s;
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
  _bev = Kmem_alloc::allocator()->unaligned_alloc(4096);
  Unsigned32 *p = (Unsigned32 *)_bev;

  unsigned cca = Mips::Cfg<0>::read().k0();

  switch (cca)
    {
    case 4: // CWBE coherent exclusive write back
    case 5: // CWB coherent write back
      break;
    default:
      printf("MIPS: error non-coherent CCA on boot cpu disbale MP\n");
      return nullptr;
    }

  p = COPY_MP_INIT(p, _tramp_mp_e0);

  // CCA -> $v0 for segctl and cache init
  *(p++) = 0x34020000 | cca; // ORI $2, $0, cca

  if (Cpu::options.segctl())
    p = COPY_MP_INIT(p, _tramp_mp_segctl);

  p = COPY_MP_INIT(p, _tramp_mp_cache);

  if (Cm::present())
    {
      // this snippet enables coherency
      // GCR base -> $v0
      *(p++) = 0x3c020000 | (Cm::cm->mmio_base() >> 16);    // LUI $2, ...
      *(p++) = 0x34420000 | (Cm::cm->mmio_base() & 0xffff); // ORI $2, $2, ...
      p = COPY_MP_INIT(p, _tramp_mp_cm);
    }

  p = COPY_MP_INIT(p, _tramp_mp_jmp_entry);

  // FIXME: use generic cache ops once implemented
  for (Unsigned32 const *s = (Unsigned32 *)_bev; s <= p; s+= 32/4)
    {
      asm volatile ("cache 0x15, %0" : : "m"(*s));
      asm volatile ("sync");
      asm volatile ("cache 0x17, %0" : : "m"(*s));
    }

  asm volatile ("sync" : : : "memory");
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
  unsigned num_cores = Cm::cm->num_cores();
  Cm *cm = Cm::cm;
  Mips_cpc *cpc = Mips_cpc::cpc;

  for (unsigned i = 1; i < num_cores; ++i)
    {
      cm->set_other_core(i);
      cm->set_co_reset_base(e);
      cm->set_co_coherence(0);
      cm->set_access(1UL << i);
      (void) cm->get_co_coherence();

      cpc->set_other_core(i);
      cpc->reset_other_core();
    }
}

