INTERFACE:

#include "cpu.h"
#include "types.h"
#include "mmio_register_block.h"

class Cm
{
public:
  enum Register
  {
    R_gcr_config            = 0x0000,
    R_gcr_base              = 0x0008,
    R_gcr_control           = 0x0010,
    R_gcr_control2          = 0x0018,
    R_gcr_access            = 0x0020,
    R_gcr_rev               = 0x0030,
    R_gcr_error_mask        = 0x0040,
    R_gcr_error_cause       = 0x0048,
    R_gcr_error_addr        = 0x0050,
    R_gcr_error_mult        = 0x0058,
    R_gcr_custom_base       = 0x0060,
    R_gcr_custom_status     = 0x0068,
    R_gcr_l2_only_sync_base = 0x0070,
    R_gcr_gic_base          = 0x0080,
    R_gcr_cpc_base          = 0x0088,
    R_gcr_reg0_base         = 0x0090,
    R_gcr_reg0_mask         = 0x0098,
    R_gcr_reg1_base         = 0x00a0,
    R_gcr_reg1_mask         = 0x00a8,
    R_gcr_reg2_base         = 0x00b0,
    R_gcr_reg2_mask         = 0x00b8,
    R_gcr_reg3_base         = 0x00c0,
    R_gcr_reg3_mask         = 0x00c8,
    R_gcr_gic_status        = 0x00d0,
    R_gcr_cache_rev         = 0x00e0,
    R_gcr_cpc_status        = 0x00f0,
    R_gcr_reg0_attr_base    = 0x0190,
    R_gcr_reg0_attr_mask    = 0x0198,
    R_gcr_reg1_attr_base    = 0x01a0,
    R_gcr_reg1_attr_mask    = 0x01a8,
    R_gcr_iocu1_rev         = 0x0200,
    R_gcr_reg2_attr_base    = 0x0210,
    R_gcr_reg2_attr_mask    = 0x0218,
    R_gcr_reg3_attr_base    = 0x0220,
    R_gcr_reg3_attr_mask    = 0x0228,

    R_gcr_cl                = 0x2000,
    R_gcr_co                = 0x4000,
    O_gcr_coherence         = 0x08,
    O_gcr_config            = 0x10,
    O_gcr_other             = 0x18,
    O_gcr_reset_base        = 0x20,
    O_gcr_id                = 0x28,
    O_gcr_reset_ext_base    = 0x30,
    O_gcr_tcid_0_priority   = 0x40,
  };
  static Static_object<Cm> cm;

  Address mmio_base() const
  { return _gcr_base.get_mmio_base(); }

  void set_gic_base_and_enable(Address a)
  { _gcr_base[R_gcr_gic_base] = a | 1; }

  bool cpc_present() const
  { return _cpc_enabled; }

  unsigned num_cores() const
  { return (_gcr_base[R_gcr_config] & 0xff) + 1; }

  void set_other_core(Mword core)
  {
    _gcr_base[R_gcr_cl + O_gcr_other] = core << 16;
  }

  void set_co_reset_base(Address base)
  {
    _gcr_base[R_gcr_co + O_gcr_reset_base] = base;
  }

  void set_cl_coherence(Mword ch)
  {
    _gcr_base[R_gcr_cl + O_gcr_coherence] = ch;
  }

  void set_co_coherence(Mword ch)
  {
    _gcr_base[R_gcr_co + O_gcr_coherence] = ch;
  }

  Unsigned32 get_cl_coherence() const
  {
    return _gcr_base[R_gcr_cl + O_gcr_coherence];
  }

  Unsigned32 get_co_coherence() const
  {
    return _gcr_base[R_gcr_co + O_gcr_coherence];
  }

  Unsigned32 del_access(Mword a)
  { return _gcr_base[R_gcr_access].clear(a); }

  Unsigned32 set_access(Mword a)
  { return _gcr_base[R_gcr_access].set(a); }

private:
  Phys_mem_addr _gcr_phys;
  Register_block<32> _gcr_base;
  bool _cpc_enabled = false;
};

// --------------------------------------------------
IMPLEMENTATION:

#include <cstdio>
#include "cpc.h"
#include "panic.h"
#include "mem_layout.h"

Static_object<Cm> Cm::cm;

PUBLIC static inline
bool
Cm::present()
{
  return Mips::Configs::read().r<3>().cmgcr();
}

PRIVATE
void
Cm::setup_cpc()
{
  Address v = cxx::int_value<Phys_mem_addr>(_gcr_phys) << 4;

  // set the CPC base address behind the GCR base
  v += 0x8000;
  _gcr_base[R_gcr_cpc_base] = v | 1;

  if (0)
    printf("MIPS: CPC base %lx\n", v);

  if ((_gcr_base[R_gcr_cpc_base] & 1) == 0)
    {
      printf("MIPS: warning could not enable CPC\n");
      return;
    }

  _cpc_enabled = true;
  auto *cpc = Mips_cpc::cpc.construct(v);
  set_cl_coherence(0xff);

  // paranoia check that the current core enters
  // coherent execution mode
  for (unsigned i = 0; i < 100; ++i)
    {
      asm volatile ("ehb" : : : "memory");
      asm volatile ("sync" : : : "memory");
      auto s = cpc->get_cl_stat_conf();
      if (((s >> 19) & 0xf) == 7)
        return;
    }

  printf("MIPS: warning boot core did not reach U6 state.\n");
}

PUBLIC
Cm::Cm()
{
  if (!present())
    {
      printf("MIPS: No Coherency Manager (CM) available.\n");
      return;
    }

  Mword v = Mips::mfc0(Mips::Cp0_cmgcr_base);

  // FIXME: allow 36bit remapping of the GCR
  if (v & 0xf0000000)
    panic("GCR base out of range (>32bit phys): %08lx\n", v);

  // FIXME: support some kind of io-reap for the kernel
  if (v >= 0x02000000)
    panic("GCR base out of unmapped KSEG(>512MB phys): %08lx\n", v);

  _gcr_phys = Phys_mem_addr(v);
  _gcr_base.set_mmio_base(Mem_layout::ioremap_nocache(v << 4, 0x8000));

  printf("MIPS: Coherency Manager (CM) found: phys=%08lx(<<4) virt=%08lx\n",
         v, _gcr_base.get_mmio_base());

  unsigned config = _gcr_base[R_gcr_config];
  printf("MIPS: CM: cores=%u iocus=%u regions=%u\n",
         (config & 0xff) + 1, (config >> 8) & 0xf, (config >> 16) & 0xf);

  if (0)
    {
      printf("MIPS: CM: gcr_base: %x\n", (unsigned)_gcr_base[R_gcr_base]);
      printf("MIPS: CM: control:  %x\n", (unsigned)_gcr_base[R_gcr_control]);
      printf("MIPS: CM: control2: %x\n", (unsigned)_gcr_base[R_gcr_control2]);
      printf("MIPS: CM: access:   %x\n", (unsigned)_gcr_base[R_gcr_access]);
    }

  if (_gcr_base[R_gcr_cpc_status] & 1)
    setup_cpc();
}
