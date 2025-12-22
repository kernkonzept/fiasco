INTERFACE [arm && pf_s32z]:

EXTENSION class Mru
{
  enum
  {
    Mru_ppi_int0_id = 17,
    Mru_ppi_int1_id = 18,
  };

  enum
  {
    Notify0 = 0x200,
    Notify1 = 0x204,
  };

  struct Cfg1
  {
    Unsigned32 raw;
    explicit Cfg1(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(16, 23, mbic, raw);
  };

public:
  enum { Nr_irqs = 12 };

  explicit Mru(void *base, Irq_chip_gen *parent)
  : Irq_chip_gen(Nr_irqs), Mmio_register_block(base),
    _ppi0(this),
    _ppi1(this)
  {
    init(Nr_irqs);

    _ppi0.init(parent, Mru_ppi_int0_id);
    _ppi1.init(parent, Mru_ppi_int1_id);
  }

private:
  Mru_ppi _ppi0;
  Mru_ppi _ppi1;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32z]:

PUBLIC
Mword
Mru::pending()
{
  Vmid_guard g;
  return read<Mword>(Notify0) | read<Mword>(Notify1);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32z && pf_s32z_mru]:

#include "amp_node.h"
#include "kip.h"
#include "kmem_mmio.h"

PUBLIC static
Mru *
Mru::create_mru(Irq_chip_gen *parent)
{
  static Address const mru_base[] = {
    Mem_layout::Rtu0_Mru0, Mem_layout::Rtu0_Mru1,
    Mem_layout::Rtu0_Mru2, Mem_layout::Rtu0_Mru3,
    Mem_layout::Rtu1_Mru0, Mem_layout::Rtu1_Mru1,
    Mem_layout::Rtu1_Mru2, Mem_layout::Rtu1_Mru3,
  };

  Address mru_addr = mru_base[cxx::int_value<Amp_phys_id>(Amp_node::phys_id())];
  Kip::k()->add_mem_region(Mem_desc(mru_addr,
                                    mru_addr + Mem_layout::Mru_size - 1,
                                    Mem_desc::Reserved, false,
                                    Mem_desc::Reserved_mmio));
  void *mru_regs = Kmem_mmio::map(mru_addr, Mem_layout::Mru_size);

  return new Boot_object<Mru>(mru_regs, parent);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32z && !pf_s32z_mru]:

PUBLIC static inline
Mru *
Mru::create_mru(Irq_chip_gen *)
{
  return nullptr;
}
