//-------------------------------------------------------------------
INTERFACE [arm && pic_gic]:

#include "gic.h"
#include "initcalls.h"
#include "global_data.h"

EXTENSION class Pic
{
public:
  static void init_ap(Cpu_number cpu, bool resume);
  static Global_data<Gic *>gic;
};

//-------------------------------------------------------------------
INTERFACE [arm && pic_gic && have_arm_gicv2 && !(dt || arm_acpi)]:

#include "mem_layout.h"

EXTENSION class Pic
{
public:
  static Address gic_cpu_phys_base() { return Mem_layout::Gic_cpu_phys_base; }
  static Address gic_dist_phys_base() { return Mem_layout::Gic_dist_phys_base; }
};

//-------------------------------------------------------------------
INTERFACE [arm && pic_gic && have_arm_gicv2 && vgic && !(dt || arm_acpi)]:

EXTENSION class Pic
{
public:
  static Address gic_h_phys_base() { return Mem_layout::Gic_h_phys_base; }
  static Address gic_v_phys_base() { return Mem_layout::Gic_v_phys_base; }
};

//-------------------------------------------------------------------
INTERFACE [arm && pic_gic && have_arm_gicv2 && (dt || arm_acpi)]:

EXTENSION class Pic
{
public:
  static Address gic_cpu_phys_base() { return _gic_cpu_phys_base; }
  static Address gic_dist_phys_base() { return _gic_dist_phys_base; }
  static Address gic_h_phys_base() { return _gic_h_phys_base; }
  static Address gic_v_phys_base() { return _gic_v_phys_base; }

private:
  static Address _gic_dist_phys_base;
  static Address _gic_cpu_phys_base;
  static Address _gic_h_phys_base;
  static Address _gic_v_phys_base;
};

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

DEFINE_GLOBAL Global_data<Gic *> Pic::gic;

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && arm_em_tz]:

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  gic->set_pending_irq(group32num, val);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp]:

IMPLEMENT_DEFAULT
void
Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}

// ------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && have_arm_gicv2 && (dt || arm_acpi)]:

Address Pic::_gic_dist_phys_base;
Address Pic::_gic_cpu_phys_base;
Address Pic::_gic_h_phys_base;
Address Pic::_gic_v_phys_base;

// ------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && dt && have_arm_gicv2]:

#include "dt.h"
#include "gic_v2.h"
#include "irq_mgr.h"
#include "kmem_mmio.h"
#include "panic.h"

PRIVATE static FIASCO_INIT
bool
Pic::init_dt_gicv2()
{
  const char *c[] = { "arm,cortex-a15-gic", "arm,gic-400", "arm,cortex-a7-gic",
                      "arm,cortex-a5-gic", "arm,cortex-a9-gic" };
  Dt::Node gic_node = Dt::node_by_compatible_list(c);
  if (!gic_node.is_valid())
    return false;

  uint64_t dist_phys_base, dist_size;
  uint64_t cpu_phys_base, cpu_size;
  bool ret = gic_node.get_reg(0, &dist_phys_base, &dist_size);
  if (!ret)
    panic("Missing distributor regs in GIC DT");
  ret = gic_node.get_reg(1, &cpu_phys_base, &cpu_size);
  if (!ret)
    panic("Missing CPU regs in GIC DT");

  _gic_dist_phys_base = dist_phys_base;
  _gic_cpu_phys_base = cpu_phys_base;

  uint64_t val, size_dummy;
  ret = gic_node.get_reg(2, &val, &size_dummy);
  if (ret)
    _gic_h_phys_base = val;

  ret = gic_node.get_reg(3, &val, &size_dummy);
  if (ret)
    _gic_v_phys_base = val;

  typedef Irq_mgr_single_chip<Gic_v2> Mgr;
  Mgr *m = new Boot_object<Mgr>(Kmem_mmio::map(cpu_phys_base, cpu_size),
                                Kmem_mmio::map(dist_phys_base, dist_size));
  gic = &m->c;
  Irq_mgr::mgr = m;

  return true;
}

// ------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && dt && !have_arm_gicv2]:

PRIVATE static inline bool Pic::init_dt_gicv2() { return false; }

// ------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && dt && have_arm_gicv3]:

#include "dt.h"
#include "gic_v3.h"
#include "irq_mgr_msi.h"
#include "kmem_mmio.h"
#include "panic.h"

class Gic_redist_find_dt : public Gic_redist_find
{
public:
  Gic_redist_find_dt(Dt::Node gic_node)
  : _gic_node(gic_node)
  {
    if (!_gic_node.get_prop_u32("#redistributor-regions", &_num_redists))
      _num_redists = 1;
  }

  Mmio_register_block get_redist_mmio(Unsigned64 mpid) override
  {
    Mmio_register_block regblock;

    _gic_node.for_each_reg([&](unsigned idx, uint64_t base, uint64_t sz)
      {
        if (idx == 0)
          return Dt::Continue; // Distributor

        // More redistributor entries?
        if (idx >= _num_redists + 1)
          return Dt::Break;

        // Assumes Kmem_mmio::map is smart and reuses existing mappings
        auto *r = Kmem_mmio::map(base, sz);
        if (!r)
          return Dt::Break;
        auto b = scan_range(r, sz, mpid);
        if (b.valid())
          {
            regblock = b;
            return Dt::Break;
          }

        return Dt::Continue;
      });

    return regblock;
  }

private:
  Dt::Node _gic_node;
  unsigned _num_redists;
};

PRIVATE static FIASCO_INIT
bool
Pic::init_dt_gicv3()
{
  Dt::Node gic_node = Dt::node_by_compatible("arm,gic-v3");
  if (!gic_node.is_valid())
    return false;

  uint64_t dist_phys, dist_size;

  if (!gic_node.get_reg(0, &dist_phys, &dist_size))
    panic("GICv3: No distributor region in device tree");

  auto *redist_find = new Boot_object<Gic_redist_find_dt>(gic_node);

  auto *g = new Boot_object<Gic_v3>(Kmem_mmio::map(dist_phys, dist_size),
                                    redist_find);

  Dt::nodes_by_compatible("arm,gic-v3-its",
                          [g](Dt::Node n)
                          {
                            uint64_t base, sz;
                            if (n.get_reg(0, &base, &sz))
                              g->add_its(Kmem_mmio::map(base, sz));
                          });

  typedef Irq_mgr_msi<Gic_v3, Gic_msi> M;
  gic = g;
  Irq_mgr::mgr = new Boot_object<M>(g, g->msi_chip());

  return true;
}

// ------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && dt && !have_arm_gicv3]:

PRIVATE static inline bool Pic::init_dt_gicv3() { return false; }

// ------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && dt]:

PRIVATE static FIASCO_INIT
void
Pic::init_dt()
{
  bool success = init_dt_gicv3();
  if (!success)
    success = init_dt_gicv2();

  if (!success)
    panic("No known interrupt controller found in DT");
}

// ------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && !dt]:

PRIVATE static FIASCO_INIT
void
Pic::init_dt()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && arm_acpi && have_arm_gicv3]:

#include "acpi.h"
#include "gic.h"
#include "gic_v3.h"
#include "gic_redist.h"
#include "irq_mgr_msi.h"
#include "kmem_mmio.h"

class Gic_redist_find_acpi : public Gic_redist_find
{
public:
  Gic_redist_find_acpi(const Acpi_madt *madt)
  : _madt(madt)
  {}

  Mmio_register_block get_redist_mmio(Unsigned64 mpid) override
  {
    bool region = false;
    for (auto const *redist : _madt->iterate<Acpi_madt::Gic_redistributor_if>())
      {
        region = true;
        // Assumes Kmem_mmio::map is smart and reuses existing mappings
        auto *r = Kmem_mmio::map(redist->base, redist->length);
        if (!r)
          return Mmio_register_block();
        auto b = scan_range(r, redist->length, mpid);
        if (b.valid())
          return b;
      }

    // if there was a Acpi_madt::Gic_redistributor_if, other options do not
    // exist per ACPI spec
    if (region)
      return Mmio_register_block();

    Unsigned64 mpid_pure = mpid & 0xff00ffffff;
    // Think: The loop gets longer and longer the more cores we boot.
    // Assuming the ACPI table is MPID ordered (which often seems the case)
    // we could remember the loop iterator and continue there...
    for (auto const *gicc : _madt->iterate<Acpi_madt::Gic_cpu_if>())
      {
        if ((gicc->flags & Acpi_madt::Gic_cpu_if::Enabled)
            && gicc->mpidr == mpid_pure)
          {
            /* The size is just the maximum that could be there, to know better
             * we'd need to map it to check Gic_redist::GICR_TYPER.vlpis and
             * then map again with the potentially bigger size */
            if (auto *r = Kmem_mmio::map(gicc->gicr_base,
                                         Gic_redist::GICR_frame_size * 4))
              return Mmio_register_block(r);

            break;
          }
      }

    return Mmio_register_block();
  }

private:
  const Acpi_madt *_madt;
};

PRIVATE static FIASCO_INIT
void
Pic::init_acpi()
{
  typedef Irq_mgr_msi<Gic_v3, Gic_msi> M;

  auto *madt = Acpi::find<Acpi_madt const *>("APIC");
  if (!madt)
    panic("SBSA: no MADT found!");

  auto *dist = madt->find<Acpi_madt::Gic_distributor_if>(0);
  if (!dist)
    panic("SBSA: no distributor defined in MADT!");

  if (dist->version < Acpi_madt::Gic_distributor_if::V3)
    panic("SBSA: GICv3 or higher expected!");

  auto *redist_find = new Boot_object<Gic_redist_find_acpi>(madt);

  auto *g =
    new Boot_object<Gic_v3>(Kmem_mmio::map(dist->base, Gic_dist::Size),
                            redist_find);

  Address constexpr Gic_its_size = 0x20000; // No size is given in ACPI
  int i = 0;
  while (auto *its = madt->find<Acpi_madt::Gic_its_if>(i++))
    g->add_its(Kmem_mmio::map(its->base, Gic_its_size));

  gic = g;
  Irq_mgr::mgr = new Boot_object<M>(g, g->msi_chip());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && !arm_acpi]:

PRIVATE static FIASCO_INIT
void
Pic::init_acpi()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && (arm_acpi || dt)]:

#include "dt.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  if (Dt::have_fdt())
    init_dt();
  else
    init_acpi();
}
