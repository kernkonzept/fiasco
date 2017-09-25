INTERFACE[arm && exynos]:

#include "types.h"

class Platform
{
public:
  struct Pkg_id
  {
    Mword mask;
    Mword val;
    unsigned soc;
    unsigned uart;
  };

  enum Soc_type
  {
    Soc_unknown = 0,
    Soc_4210,
    Soc_4412,
    Soc_5250,
    Soc_5410,
  };

  enum Gic_type
  {
    Int_gic, Ext_gic,
  };

  static bool gic_ext() { return gic_type() == Ext_gic; }
  static bool gic_int() { return gic_type() == Int_gic; }

private:
  static Soc_type _soc;
  static unsigned _uart;
  static unsigned _subrev;
};

// ------------------------------------------------------------------------
INTERFACE [arm && exynos && exynos_extgic]:

EXTENSION class Platform
{
public:
  static Gic_type gic_type() { return Ext_gic; }
};

// ------------------------------------------------------------------------
INTERFACE [arm && exynos && !exynos_extgic]:

EXTENSION class Platform
{
public:
  static Gic_type gic_type() { return Int_gic; }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && exynos]:

#include "io.h"
#include "mem_layout.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && exynos && exynos_extgic]:

#include <feature.h>
KIP_KERNEL_FEATURE("exy:extgic");

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && exynos]:

#include "kmem.h"

Platform::Soc_type Platform::_soc;
unsigned Platform::_subrev;
unsigned Platform::_uart;

PUBLIC static
unsigned
Platform::subrev()
{
  return _subrev;
}

//-------------------------------------------------------------------------------------------
IMPLEMENTATION [arm && exynos && pkg_id_file]:

#include "types.h"
#include "globalconfig.h"

#include CONFIG_PF_EXYNOS_PKG_IDS

PRIVATE static
void
Platform::process_pkg_ids()
{
  Mword pkg_id = Io::read<Mword>(Kmem::mmio_remap(Mem_layout::Chip_id_phys_base + 4));
  for  (unsigned i = 0; i < sizeof(__pkg_ids) / sizeof(__pkg_ids[0]); ++i)
    if ((pkg_id & __pkg_ids[i].mask) == __pkg_ids[i].val)
      {
        _soc = (Soc_type)__pkg_ids[i].soc;
        _uart = __pkg_ids[i].uart;
        return;
      }
}

//-------------------------------------------------------------------------------------------
IMPLEMENTATION [arm && exynos && !pkg_id_file]:

#include "types.h"
#include "globalconfig.h"

PRIVATE static inline
void
Platform::process_pkg_ids()
{}

//-------------------------------------------------------------------------------------------
IMPLEMENTATION [arm && exynos]:

#include "config.h"

PRIVATE static
void
Platform::type()
{
  if (_soc == 0)
    {
      Mword pro_id = Io::read<Mword>(Kmem::mmio_remap(Mem_layout::Chip_id_phys_base));

      _subrev = pro_id & 0xff;

      // set defaults from config
      _uart = Config::Uart_nr;
#if defined(CONFIG_PF_EXYNOS4_4210)
      _soc = Soc_4210;
#elif defined(CONFIG_PF_EXYNOS4_4412)
      _soc = Soc_4412;
#elif defined(CONFIG_PF_EXYNOS5_5250)
      _soc = Soc_5250;
#endif

      process_pkg_ids();
    }
}

PUBLIC static inline
bool
Platform::is_4210()
{ type(); return _soc == Soc_4210; }

PUBLIC static inline
bool
Platform::is_4412()
{ type(); return _soc == Soc_4412; }

PUBLIC static inline
bool
Platform::is_5250()
{ type(); return _soc == Soc_5250; }

PUBLIC static inline
bool
Platform::is_5410()
{ type(); return _soc == Soc_5410; }

PUBLIC static unsigned Platform::uart_nr() { type(); return _uart; }


// ------------------------------------------------------------------------
// this is actually a config option...
IMPLEMENTATION [arm && arm_em_tz]:

PUBLIC static inline
bool Platform::running_ns() { return false; }

IMPLEMENTATION [arm && arm_em_ns]: // -------------------------------------------

PUBLIC static inline
bool Platform::running_ns() { return true; }

IMPLEMENTATION [arm && arm_em_std]: // -------------------------------------------

PUBLIC static inline
bool Platform::running_ns() { return false; }

