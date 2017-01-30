INTERFACE [arm && cpu_virt && vgic]:

#include "kmem.h"
#include "mmio_register_block.h"
#include "types.h"

#include <cxx/bitfield>

class Gic_h : private Mmio_register_block
{
public:
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

  struct Hcr
  {
    Unsigned32 raw;
    Hcr() = default;
    explicit Hcr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  0, en, raw);
    CXX_BITFIELD_MEMBER(  1,  1, uie, raw);
    CXX_BITFIELD_MEMBER(  2,  2, lr_en_pie, raw);
    CXX_BITFIELD_MEMBER(  3,  3, n_pie, raw);
    CXX_BITFIELD_MEMBER(  4,  4, vgrp0_eie, raw);
    CXX_BITFIELD_MEMBER(  5,  5, vgrp0_die, raw);
    CXX_BITFIELD_MEMBER(  6,  6, vgrp1_eie, raw);
    CXX_BITFIELD_MEMBER(  7,  7, vgrp1_die, raw);
    CXX_BITFIELD_MEMBER( 27, 31, eoi_cnt, raw);
  };

  struct Vtr
  {
    Unsigned32 raw;
    Vtr() = default;
    explicit Vtr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  5, list_regs, raw);
    CXX_BITFIELD_MEMBER( 26, 28, pre_bits, raw);
    CXX_BITFIELD_MEMBER( 29, 31, pri_bits, raw);
  };

  struct Vmcr
  {
    Unsigned32 raw;
    Vmcr() = default;
    explicit Vmcr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  0, grp0_en, raw);
    CXX_BITFIELD_MEMBER(  1,  1, grp1_en, raw);
    CXX_BITFIELD_MEMBER(  2,  2, ack_ctl, raw);
    CXX_BITFIELD_MEMBER(  3,  3, fiq_en, raw);
    CXX_BITFIELD_MEMBER(  4,  4, cbpr, raw);
    CXX_BITFIELD_MEMBER(  9,  9, vem, raw);
    CXX_BITFIELD_MEMBER( 18, 20, abp, raw);
    CXX_BITFIELD_MEMBER( 21, 23, bp, raw);
    CXX_BITFIELD_MEMBER( 27, 31, pri_mask, raw);
  };

  struct Misr
  {
    Unsigned32 raw;
    Misr() = default;
    explicit Misr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  0, eoi, raw);
    CXX_BITFIELD_MEMBER(  1,  1, u, raw);
    CXX_BITFIELD_MEMBER(  2,  2, lrenp, raw);
    CXX_BITFIELD_MEMBER(  3,  3, np, raw);
    CXX_BITFIELD_MEMBER(  4,  4, grp0_e, raw);
    CXX_BITFIELD_MEMBER(  5,  5, grp0_d, raw);
    CXX_BITFIELD_MEMBER(  6,  6, grp1_e, raw);
    CXX_BITFIELD_MEMBER(  7,  7, grp1_d, raw);
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


  explicit Gic_h(Address va) : Mmio_register_block(va) {}

  static Static_object<Gic_h> gic;
};

template< unsigned LREGS >
struct Arm_vgic_t
{
   enum { N_lregs = LREGS };
   Gic_h::Hcr hcr;
   Gic_h::Vtr vtr;
   Gic_h::Vmcr vmcr;
   Gic_h::Misr misr;
   Unsigned32 eisr[2];
   Unsigned32 elsr[2];
   Unsigned32 apr;
   Gic_h::Lr lr[LREGS];
};

//------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && vgic]:

#include "per_cpu_data.h"

Static_object<Gic_h> Gic_h::gic;


PUBLIC inline Gic_h::Hcr  Gic_h::hcr()  { return Hcr(read<Unsigned32>(HCR)); }
PUBLIC inline void Gic_h::hcr(Gic_h::Hcr hcr)  { write(hcr.raw, HCR); }
PUBLIC inline Gic_h::Vtr  Gic_h::vtr()  { return Vtr(read<Unsigned32>(VTR)); }
PUBLIC inline Gic_h::Vmcr Gic_h::vmcr() { return Vmcr(read<Unsigned32>(VMCR)); }
PUBLIC inline void Gic_h::vmcr(Gic_h::Vmcr vmcr) { write(vmcr.raw, VMCR); }
PUBLIC inline Gic_h::Misr Gic_h::misr() { return Misr(read<Unsigned32>(MISR)); }
PUBLIC inline Unsigned32  Gic_h::eisr(unsigned n) { return read<Unsigned32>(EISRn + (n << 2)); }
PUBLIC inline Unsigned32  Gic_h::elsr(unsigned n) { return read<Unsigned32>(ELSRn + (n << 2)); }
PUBLIC inline Unsigned32  Gic_h::apr() { return read<Unsigned32>(APR); }
PUBLIC inline void Gic_h::apr(Unsigned32 apr) { write(apr, APR); }
PUBLIC inline Gic_h::Lr   Gic_h::lr(unsigned n) { return Lr(read<Unsigned32>(LRn + (n << 2))); }
PUBLIC inline void Gic_h::lr(unsigned n, Gic_h::Lr lr) { write(lr.raw, LRn + (n << 2)); }

struct Gic_h_init
{
  explicit Gic_h_init(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      {
        Gic_h::gic.construct(Kmem::mmio_remap(Mem_layout::Gic_h_phys_base));
      }
  }
};

namespace {
DEFINE_PER_CPU Per_cpu<Gic_h_init> __gic_h(Per_cpu_data::Cpu_num);
}

