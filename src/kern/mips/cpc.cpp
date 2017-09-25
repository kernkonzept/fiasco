INTERFACE:

#include "types.h"
#include "mmio_register_block.h"

class Mips_cpc
{
public:
  enum Register
  {
    R_access        =  0x000,
    R_seqdel        =  0x008,
    R_rail          =  0x010,
    R_resetlen      =  0x018,
    R_revision      =  0x020,

    R_cl_cmd        = 0x2000,
    R_cl_stat_conf  = 0x2008,
    R_cl_other      = 0x2010,

    R_co_cmd        = 0x4000,
    R_co_stat_conf  = 0x4008,
  };

  static Static_object<Mips_cpc> cpc;

  void set_other_core(Mword core)
  {
    _r[R_cl_other] = core << 16;
  }

  void reset_other_core()
  {
    _r[R_co_cmd] = 4;
  }

  void power_up_other_core()
  {
    _r[R_co_cmd] = 3;
  }

  Unsigned32 get_other_stat_conf()
  { return _r[R_co_stat_conf]; }

  Unsigned32 get_cl_stat_conf()
  { return _r[R_cl_stat_conf]; }

private:
  Register_block<32> _r;
};

// ----------------------------------------------------------------------
IMPLEMENTATION:

#include "kmem.h"
#include <cstdio>

Static_object<Mips_cpc> Mips_cpc::cpc;

PUBLIC explicit
Mips_cpc::Mips_cpc(Address p_base)
: _r(Kmem::mmio_remap(p_base))
{
  Unsigned32 rev = _r[R_revision];
  printf("MIPS: CPC revision %u.%u\n", (rev >> 8) & 0xff, rev & 0xff);
  if (0)
    printf("MIPS: CPC state: %x\n", (unsigned)_r[R_cl_stat_conf]);
}

