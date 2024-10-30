INTERFACE [cpu_virt && vgic]:

#include "types.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"
#include "vgic.h"

#include <cxx/bitfield>

class Gic_h_v2 :
  public Gic_h_mixin<Gic_h_v2>,
  private Mmio_register_block
{
public:
  enum { Version = 2 };

  // With GICv2, the kernel has to sanitize the vGIC state on *every* context
  // switch. When switching to a thread in extended vCPU mode, the saved vGIC
  // state is loaded. Otherwise, disable_load_defaults() loads default values
  // into the vGIC state.

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
    enum State
    {
      Empty              = 0,
      Pending            = 1,
      Active             = 2,
      Active_and_pending = 3
    };

    Unsigned32 raw;
    Lr() = default;
    explicit Lr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  9, vid, raw);
    CXX_BITFIELD_MEMBER( 10, 19, pid, raw);
    CXX_BITFIELD_MEMBER( 10, 12, cpuid, raw);
    CXX_BITFIELD_MEMBER( 19, 19, eoi, raw);
    CXX_BITFIELD_MEMBER( 23, 27, prio, raw);
    CXX_BITFIELD_MEMBER( 28, 29, state, raw);
    CXX_BITFIELD_MEMBER( 28, 28, pending, raw);
    CXX_BITFIELD_MEMBER( 29, 29, active, raw);
    CXX_BITFIELD_MEMBER( 30, 30, grp1, raw);
    CXX_BITFIELD_MEMBER( 31, 31, hw, raw);
  };

  explicit Gic_h_v2(void *va) : Mmio_register_block(va) {}

  static Static_object<Gic_h_v2> gic;
};

//------------------------------------------------------------------------
IMPLEMENTATION  [cpu_virt && vgic]:

#include "mem.h"

Static_object<Gic_h_v2> Gic_h_v2::gic;

PUBLIC inline Gic_h_v2::Hcr  Gic_h_v2::hcr() const { return Hcr(read<Unsigned32>(HCR)); }
PUBLIC inline void Gic_h_v2::hcr(Gic_h_v2::Hcr hcr)  { write(hcr.raw, HCR); }
PUBLIC inline Gic_h_v2::Vtr  Gic_h_v2::vtr() const { return Vtr(read<Unsigned32>(VTR)); }
PUBLIC inline Gic_h_v2::Vmcr Gic_h_v2::vmcr() const { return Vmcr(read<Unsigned32>(VMCR)); }
PUBLIC inline void Gic_h_v2::vmcr(Gic_h_v2::Vmcr vmcr) { write(vmcr.raw, VMCR); }
PUBLIC inline Gic_h_v2::Misr Gic_h_v2::misr() const { return Misr(read<Unsigned32>(MISR)); }
PUBLIC inline Unsigned32  Gic_h_v2::eisr() const { return read<Unsigned32>(EISRn); }
PUBLIC inline Unsigned32  Gic_h_v2::elsr() const { return read<Unsigned32>(ELSRn); }

PUBLIC inline
void
Gic_h_v2::save_aprs(Unsigned32 *a) const
{ a[0] = read<Unsigned32>(APR); }

PUBLIC inline
void
Gic_h_v2::load_aprs(Unsigned32 const *a)
{ write(a[0], APR); }

PUBLIC inline
void
Gic_h_v2::save_lrs(Gic_h::Arm_vgic::Lrs *l) const
{
  for (unsigned i = 0; i < Gic_h::Arm_vgic::N_lregs; ++i)
    l->lr32[i] = read<Unsigned32>(LRn + (i << 2));
}

PUBLIC inline
void
Gic_h_v2::load_lrs(Gic_h::Arm_vgic::Lrs const *l)
{
  for (unsigned i = 0; i < Gic_h::Arm_vgic::N_lregs; ++i)
    write(l->lr32[i], LRn + (i << 2));
}

PUBLIC inline void
Gic_h_v2::build_lr(Gic_h::Arm_vgic::Lrs *lr, unsigned idx,
                   Gic_h::Vcpu_irq_cfg cfg, bool load)
{
  Lr new_lr(0);
  new_lr.state() = Lr::Pending;
  new_lr.eoi()   = 1; // need an EOI IRQ
  new_lr.vid()   = cfg.vid();
  new_lr.prio()  = cfg.prio();
  new_lr.grp1()  = cfg.grp1();

  lr->lr32[idx] = new_lr.raw;
  if (load)
    write(new_lr.raw, LRn + (idx << 2));
}

PUBLIC inline void
Gic_h_v2::clear_lr(unsigned idx)
{
  write(0U, LRn + (idx << 2));
}

PUBLIC inline bool
Gic_h_v2::teardown_lr(Gic_h::Arm_vgic::Lrs *lr, unsigned idx)
{
  Lr reg(lr->lr32[idx]);
  if (!reg.active())
    {
      lr->lr32[idx] = 0;
      return false;
    }
  else
    return true;
}

PUBLIC
Address
Gic_h_v2::gic_v_address() const override
{ return Mem_layout::Gic_v_phys_base; }

PUBLIC static inline NEEDS["mem.h"]
void
Gic_h_v2::vgic_barrier()
{ Mem::dsb(); /* Ensure vgic completion before running user-land */ }

PUBLIC
void
Gic_h_v2::switch_to_non_vcpu(From_vgic_mode) override
{
  disable_load_defaults();
  vgic_barrier();
}

PUBLIC
void
Gic_h_v2::switch_to_vcpu(Arm_vgic const *g, To_user_mode to_mode,
                         From_vgic_mode) override
{
  if (to_mode == To_user_mode::Disabled || !switch_to_vcpu_user(g))
    {
      disable_load_defaults();
      vgic_barrier();
    }
}

PUBLIC
void
Gic_h_v2::save_and_disable(Arm_vgic *g) override
{
  switch_from_vcpu(g);
  disable_load_defaults();
  vgic_barrier();
}

PUBLIC inline
void
Gic_h_v2::disable_load_defaults()
{
  write(0U, HCR);
  write(0U, VMCR);
  write(0U, APR);
}

#include "boot_alloc.h"
#include "vgic_global.h"
#include "pic.h"

namespace {

struct Gic_h_v2_init
{
  explicit Gic_h_v2_init()
  {
    if (Pic::gic->gic_version() > 2)
      return;

    Gic_h_global::gic
      = new Boot_object<Gic_h_v2>(Kmem_mmio::map(Mem_layout::Gic_h_phys_base,
                                                 Config::PAGE_SIZE));

    Gic_h_global::gic->disable();
  }
};

Gic_h_v2_init __gic_h;
}
