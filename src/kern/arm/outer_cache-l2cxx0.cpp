INTERFACE [arm && outer_cache_l2cxx0]:

#include "lock_guard.h"
#include "mem_layout.h"
#include "spin_lock.h"
#include "mmio_register_block.h"

EXTENSION class Outer_cache
{
private:

  class L2cxx0 : public Mmio_register_block
  {
  public:
    enum
    {
      CACHE_ID                       = 0x000,
      CACHE_TYPE                     = 0x004,
      CONTROL                        = 0x100,
      AUX_CONTROL                    = 0x104,
      TAG_RAM_CONTROL                = 0x108,
      DATA_RAM_CONTROL               = 0x10c,
      EVENT_COUNTER_CONTROL          = 0x200,
      EVENT_COUTNER1_CONFIG          = 0x204,
      EVENT_COUNTER0_CONFIG          = 0x208,
      EVENT_COUNTER1_VALUE           = 0x20c,
      EVENT_COUNTER0_VALUE           = 0x210,
      INTERRUPT_MASK                 = 0x214,
      MASKED_INTERRUPT_STATUS        = 0x218,
      RAW_INTERRUPT_STATUS           = 0x21c,
      INTERRUPT_CLEAR                = 0x220,
      CACHE_SYNC                     = 0x730,
      INVALIDATE_LINE_BY_PA          = 0x770,
      INVALIDATE_BY_WAY              = 0x77c,
      CLEAN_LINE_BY_PA               = 0x7b0,
      CLEAN_LINE_BY_INDEXWAY         = 0x7bb,
      CLEAN_BY_WAY                   = 0x7bc,
      CLEAN_AND_INV_LINE_BY_PA       = 0x7f0,
      CLEAN_AND_INV_LINE_BY_INDEXWAY = 0x7f8,
      CLEAN_AND_INV_BY_WAY           = 0x7fc,
      LOCKDOWN_BY_WAY_D_SIDE         = 0x900,
      LOCKDOWN_BY_WAY_I_SIDE         = 0x904,
      TEST_OPERATION                 = 0xf00,
      LINE_TAG                       = 0xf30,
      DEBUG_CONTROL_REGISTER         = 0xf40,
    };

    enum
    {
      Pwr_ctrl_standby_mode_en       = 1 << 0,
      Pwr_ctrl_dynamic_clk_gating_en = 1 << 1,
    };

    Spin_lock<> _lock;

    explicit L2cxx0(Address virt) : Mmio_register_block(virt) {}

    void write_op(Address reg, Mword val)
    {
      Mmio_register_block::write<Mword>(val, reg);
      while (read<Mword>(reg) & 1)
        ;
    }

    void write_way_op(Address reg, Mword val)
    {
      Mmio_register_block::write<Mword>(val, reg);
      while (read<Mword>(reg) & val)
        ;
    }
  };

  static Mword platform_init(Mword aux);

  static Static_object<L2cxx0> l2cxx0;
  static bool need_sync;
  static unsigned waymask;

public:
  enum
  {
    Cache_line_shift = 5,
    Cache_line_size = 1 << Cache_line_shift,
    Cache_line_mask = Cache_line_size - 1,
  };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && outer_cache_l2cxx0]:

#include "kmem.h"
#include "processor.h"
#include "static_init.h"

Static_object<Outer_cache::L2cxx0> Outer_cache::l2cxx0;

bool Outer_cache::need_sync;
unsigned Outer_cache::waymask;

IMPLEMENT inline
void
Outer_cache::sync()
{
  while (l2cxx0->read<Mword>(L2cxx0::CACHE_SYNC))
    Proc::preemption_point();
}

IMPLEMENT inline
void
Outer_cache::clean()
{
  auto guard = lock_guard(l2cxx0->_lock);
  l2cxx0->write_way_op(L2cxx0::CLEAN_BY_WAY, waymask);
  sync();
}

IMPLEMENT inline
void
Outer_cache::clean(Mword phys_addr, bool do_sync = true)
{
  auto guard = lock_guard(l2cxx0->_lock);
  l2cxx0->write_op(L2cxx0::CLEAN_LINE_BY_PA, phys_addr & (~0UL << Cache_line_shift));
  if (need_sync && do_sync)
    sync();
}

IMPLEMENT inline
void
Outer_cache::flush()
{
  auto guard = lock_guard(l2cxx0->_lock);
  l2cxx0->write_way_op(L2cxx0::CLEAN_AND_INV_BY_WAY, waymask);
  sync();
}

IMPLEMENT inline
void
Outer_cache::flush(Mword phys_addr, bool do_sync = true)
{
  auto guard = lock_guard(l2cxx0->_lock);
  l2cxx0->write_op(L2cxx0::CLEAN_AND_INV_LINE_BY_PA, phys_addr & (~0UL << Cache_line_shift));
  if (need_sync && do_sync)
    sync();
}

IMPLEMENT inline
void
Outer_cache::invalidate()
{
  auto guard = lock_guard(l2cxx0->_lock);
  l2cxx0->write_way_op(L2cxx0::INVALIDATE_BY_WAY, waymask);
  sync();
}

IMPLEMENT inline
void
Outer_cache::invalidate(Address phys_addr, bool do_sync = true)
{
  auto guard = lock_guard(l2cxx0->_lock);
  l2cxx0->write_op(L2cxx0::INVALIDATE_LINE_BY_PA, phys_addr & (~0UL << Cache_line_shift));
  if (need_sync && do_sync)
    sync();
}

PUBLIC static
void
Outer_cache::initialize(bool v)
{
  l2cxx0.construct(Kmem::mmio_remap(Mem_layout::L2cxx0_phys_base));
  Mword cache_id   = l2cxx0->read<Mword>(L2cxx0::CACHE_ID);
  Mword aux        = l2cxx0->read<Mword>(L2cxx0::AUX_CONTROL);
  unsigned ways    = 8;

  need_sync = true;

  aux = platform_init(aux);

  switch ((cache_id >> 6) & 0xf)
    {
    case 1:
      ways = (aux >> 13) & 0xf;
      break;
    case 3:
      need_sync = false;
      ways = aux & (1 << 16) ? 16 : 8;
      break;
    default:
      break;
    }

  waymask = (1 << ways) - 1;

  l2cxx0->write<Mword>(0, L2cxx0::INTERRUPT_MASK);
  l2cxx0->write<Mword>(~0UL, L2cxx0::INTERRUPT_CLEAR);

  if (!(l2cxx0->read<Mword>(L2cxx0::CONTROL) & 1))
    {
      l2cxx0->write(aux, L2cxx0::AUX_CONTROL);
      invalidate();
      l2cxx0->write<Mword>(1, L2cxx0::CONTROL);
    }

  platform_init_post();

  if (v)
    show_info(ways, cache_id, aux);
}

PUBLIC static
void
Outer_cache::init()
{
  initialize(true);
}

STATIC_INITIALIZE_P(Outer_cache, STARTUP_INIT_PRIO);

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && outer_cache_l2cxx0 && !debug]:

PRIVATE static
void
Outer_cache::show_info(unsigned, Mword, Mword)
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && outer_cache_l2cxx0 && debug]:

#include "io.h"
#include <cstdio>

PRIVATE static
void
Outer_cache::show_info(unsigned ways, Mword cache_id, Mword aux)
{
  printf("L2: ID=%08lx Type=%08lx Aux=%08lx WMask=%x S=%d\n",
         cache_id, l2cxx0->read<Mword>(L2cxx0::CACHE_TYPE), aux, waymask, need_sync);

  const char *type;
  switch ((cache_id >> 6) & 0xf)
    {
    case 1:
      type = "210";
      break;
    case 2:
      type = "220";
      break;
    case 3:
      type = "310";
      if ((cache_id & 0x3f) == 5)
        printf("L2: r3p0\n");
      break;
    default:
      type = "Unknown";
      break;
    }

  unsigned waysize = 16 << (((aux >> 17) & 7) - 1);
  printf("L2: Type L2C-%s Size = %dkB  Ways=%d Waysize=%d\n",
         type, ways * waysize, ways, waysize);
}
