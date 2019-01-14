INTERFACE [cpu_virt && vgic]:

#include "types.h"
#include "kmem.h"
#include "mmio_register_block.h"

#include <cxx/bitfield>

EXTENSION class Gic_h : private Mmio_register_block
{
public:
  enum { Version = 2 };
  enum Register
  {
    HCR   = 0x000,
    VTR   = 0x004,
    VMCR  = 0x008,
    MISR  = 0x010,

    EISRn = 0x020,
    EISR0 = 0x020,
    EISR1 = 0x024,

    ELSRn = 0x030,
    ELSR0 = 0x030,
    ELSR1 = 0x034,

    APR   = 0x0f0,

    LRn   = 0x100,
    LR0   = LRn,
    LR63  = 0x1fc
  };

  struct Lr
  {
    Unsigned32 raw;
    Lr() = default;
    explicit Lr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  9, vid, raw);
    CXX_BITFIELD_MEMBER( 10, 19, pid, raw);
    CXX_BITFIELD_MEMBER( 10, 12, cpuid, raw);
    CXX_BITFIELD_MEMBER( 19, 19, eoi, raw);
    CXX_BITFIELD_MEMBER( 23, 27, prio, raw);
    CXX_BITFIELD_MEMBER( 28, 29, state, raw);
    CXX_BITFIELD_MEMBER( 30, 30, grp1, raw);
    CXX_BITFIELD_MEMBER( 31, 31, hw, raw);
  };

  struct Aprs
  {
    void clear() { apr = 0; }
    Unsigned32 apr;
  };

  explicit Gic_h(Address va) : Mmio_register_block(va) {}

  static Static_object<Gic_h> gic;
};

//------------------------------------------------------------------------
IMPLEMENTATION  [cpu_virt && vgic]:

Static_object<Gic_h> Gic_h::gic;

PUBLIC inline Gic_h::Hcr  Gic_h::hcr()  { return Hcr(read<Unsigned32>(HCR)); }
PUBLIC inline void Gic_h::hcr(Gic_h::Hcr hcr)  { write(hcr.raw, HCR); }
PUBLIC inline Gic_h::Vtr  Gic_h::vtr()  { return Vtr(read<Unsigned32>(VTR)); }
PUBLIC inline Gic_h::Vmcr Gic_h::vmcr() { return Vmcr(read<Unsigned32>(VMCR)); }
PUBLIC inline void Gic_h::vmcr(Gic_h::Vmcr vmcr) { write(vmcr.raw, VMCR); }
PUBLIC inline Gic_h::Misr Gic_h::misr() { return Misr(read<Unsigned32>(MISR)); }
PUBLIC inline Unsigned32  Gic_h::eisr(unsigned n) { return read<Unsigned32>(EISRn + (n << 2)); }
PUBLIC inline Unsigned32  Gic_h::elsr(unsigned n) { return read<Unsigned32>(ELSRn + (n << 2)); }

PUBLIC inline void
Gic_h::save_aprs(Gic_h::Aprs *a)
{ a->apr = read<Unsigned32>(APR); }

PUBLIC inline void
Gic_h::load_aprs(Gic_h::Aprs const *a)
{ write(a->apr, APR); }

PUBLIC inline void
Gic_h::save_lrs(Gic_h::Lr *l, unsigned n)
{
  for (unsigned i = 0; i < n; ++i)
    l[i] = Lr(read<Unsigned32>(LRn + (i << 2)));
}

PUBLIC inline void
Gic_h::load_lrs(Gic_h::Lr const *l, unsigned n)
{
  for (unsigned i = 0; i < n; ++i)
    write(l[i].raw, LRn + (i << 2));
}


namespace {

struct Gic_h_init
{
  explicit Gic_h_init()
  {
    Gic_h::gic.construct(Kmem::mmio_remap(Mem_layout::Gic_h_phys_base));
  }
};

Gic_h_init __gic_h;
}

