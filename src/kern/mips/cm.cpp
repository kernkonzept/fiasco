INTERFACE:

#include <cxx/bitfield>
#include "cpu.h"
#include "types.h"
#include "mem.h"
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
    R_gcr_l2_config         = 0x0130,
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

    O_gcr_cpc_offset        = 0x8000,
    R_cpc_access            = O_gcr_cpc_offset + 0x000,
    R_cpc_seqdel            = O_gcr_cpc_offset + 0x008,
    R_cpc_rail              = O_gcr_cpc_offset + 0x010,
    R_cpc_resetlen          = O_gcr_cpc_offset + 0x018,
    R_cpc_revision          = O_gcr_cpc_offset + 0x020,
    R_cpc_cl                = O_gcr_cpc_offset + 0x2000,
    R_cpc_co                = O_gcr_cpc_offset + 0x4000,

    O_cpc_cmd               = 0x00,
    O_cpc_stat_conf         = 0x08,
    O_cpc_other             = 0x10,
  };

  enum Cm_revisions
  {
    Rev_cm2 = 6,
    Rev_cm2_5 = 7,
    Rev_cm3 = 8,
    Rev_cm3_5 = 9
  };

  static Cm *cm;

  Cm(unsigned revision, Phys_mem_addr phys)
  : _gcr_phys(phys), _rev(revision)
  {}

  virtual void start_all_vps(Address e) = 0;
  virtual void set_gic_base_and_enable(Address a) = 0;
  virtual Address mmio_base() const = 0;
  virtual unsigned l2_cache_line() const = 0;

  unsigned revision() const
  { return _rev; }

  bool cpc_present() const
  { return _cpc_enabled; }

protected:
  struct Cpc_stat_conf
  {
    Unsigned32 v;
    Cpc_stat_conf() = default;
    explicit Cpc_stat_conf(Unsigned32 v) : v(v) {}

    CXX_BITFIELD_MEMBER( 0,  3, cmd, v);
    CXX_BITFIELD_MEMBER( 4,  4, io_trffc_en, v);
    CXX_BITFIELD_MEMBER( 7,  7, reset_hold, v);     // CM3
    CXX_BITFIELD_MEMBER( 8,  9, pwup_policy, v);
    CXX_BITFIELD_MEMBER(10, 10, lpack, v);          // CM3
    CXX_BITFIELD_MEMBER(11, 11, coh_en, v);         // CM3
    CXX_BITFIELD_MEMBER(12, 12, ci_rail_stable, v); // CM3
    CXX_BITFIELD_MEMBER(13, 13, ci_vdd_ok, v);      // CM3
    CXX_BITFIELD_MEMBER(14, 14, ci_pwrup, v);       // CM3
    CXX_BITFIELD_MEMBER(15, 15, ejtag_probe, v);
    CXX_BITFIELD_MEMBER(16, 16, pwrdn_impl, v);
    CXX_BITFIELD_MEMBER(17, 17, clkgat_impl, v);
    CXX_BITFIELD_MEMBER(19, 22, seq_state, v);
    CXX_BITFIELD_MEMBER(23, 23, pwrup_event, v);
    CXX_BITFIELD_MEMBER(24, 24, l2_hw_init_en, v);
  };

  virtual void setup_cpc() = 0;

  Phys_mem_addr _gcr_phys;
  unsigned _rev;
  bool _cpc_enabled = false;
};

template<unsigned REG_WIDTH>
class Cm_x : public Cm
{
public:

  Address mmio_base() const override
  { return _gcr_base.get_mmio_base(); }

  void set_gic_base_and_enable(Address a) override
  { _gcr_base[R_gcr_gic_base] = a | 1; }


  unsigned num_cores() const
  { return (_gcr_base[R_gcr_config] & 0xff) + 1; }


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

  void reset_other_core()
  {
    _gcr_base[R_cpc_co + O_cpc_cmd] = 4;
  }

  void power_up_other_core()
  {
    _gcr_base[R_cpc_co + O_cpc_cmd] = 3;
  }

protected:
  Register_block<REG_WIDTH> _gcr_base;

  Cpc_stat_conf get_other_stat_conf() const
  { return Cpc_stat_conf(_gcr_base[R_cpc_co + O_cpc_stat_conf]); }

  Cpc_stat_conf get_stat_conf() const
  { return Cpc_stat_conf(_gcr_base[R_cpc_cl + O_cpc_stat_conf]); }
};


class Cm2 : public Cm_x<32>
{
public:
  Cm2(unsigned revision, Phys_mem_addr phys, Address base)
  : Cm_x<32>(revision, phys, base)
  {}

  void start_all_vps(Address e) override
  {
    unsigned cores = num_cores();
    for (unsigned i = 1; i < cores; ++i)
      {
        set_other_core(i);
        set_co_reset_base(e);
        set_co_coherence(0);
        set_access(1UL << i);
        Mem::sync();
        (void) get_co_coherence();
        reset_other_core();
        Mem::sync();
      }
  }

  unsigned l2_cache_line() const override
  {
    return 0; // L2 cache not managed by CM2
  }

private:
  void set_other_core(Mword core)
  {
    _gcr_base[R_gcr_cl + O_gcr_other] = core << 16;
    if (_cpc_enabled)
      _gcr_base[R_cpc_cl + O_cpc_other] = core << 16;

    Mem::sync();
  }
};

class Cm3 : public Cm_x<MWORD_BITS>
{
public:
  enum Register_cm3
  {
    O_cpc_ctl_reg    = 0x18,
    O_cpc_vp_stop    = 0x20,
    O_cpc_vp_run     = 0x28,
    O_cpc_vp_running = 0x30,
    O_cpc_ram_sleep  = 0x50,
  };

  Cm3(unsigned revision, Phys_mem_addr phys, Address base)
  : Cm_x<MWORD_BITS>(revision, phys, base)
  {}

  void start_all_vps(Address e) override
  {
    Unsigned32 myself = Mips::mfc0_32(Mips::Cp0_global_number);
    unsigned my_vp = myself & 0xff;
    unsigned cores = num_cores();

    for (unsigned i = 0; i < cores; ++i)
      {
        Mword other = redirect(i, 0);
        set_other_core(other);
        set_access(1UL << i);

        auto stat = get_other_stat_conf();
        bool need_reset = false;
        if (stat.seq_state() != 7)
          {
            set_co_coherence(0);
            need_reset = true;
          }

        set_co_reset_base(e);
        unsigned nvps = (_gcr_base[R_gcr_co + O_gcr_config] & 0x3f) + 1;

        for (unsigned v = 1; v < nvps; ++v)
          {
            Mem::sync();
            set_other_core(redirect(i, v));
            set_co_reset_base(e);
          }

        Mem::sync();
        if (nvps > 1)
          set_other_core(redirect(i, 0));

        (void) get_co_coherence();

        unsigned vps_to_reset = (1UL << nvps) - 1;
        if ((myself & ~0x0ff) == redirect(i, 0))
          {
            // start all other VPs on the current core
            vps_to_reset &= ~(1UL << my_vp);
            if (vps_to_reset)
              {
                stop_other_vp(vps_to_reset);
                Mem::sync();
                run_other_vp(vps_to_reset);
              }

            // done with the current core
            continue;
          }

        // stop all VPs on the other core
        stop_other_vp(vps_to_reset);
        Mem::sync();

        // if the other core is not yet in coherent state do a reset
        if (need_reset)
          {
            reset_other_core();
            Mem::sync();
            // make sure all VPs are stopped, I'm not sure if this is needed
            stop_other_vp(vps_to_reset);
            Mem::sync();
          }

        // make sure no VP is running on the core to boot
        while (vps_to_reset & other_running())
          Mem::sync();

        // start VP 0 of the core, this VP will then enable coherency and
        // start the other VPs after it initialized the caches and enabled
        // coherency
        run_other_vp(1); 
        Mem::sync();
      }
  }

  unsigned l2_cache_line() const override
  {
    return (_gcr_base[R_gcr_l2_config] >> 8) & 0xf;
  }

private:
  Mword redirect(unsigned core, unsigned vp)
  { return (core << 8) | vp; }

  void set_other_core(Mword core)
  {
    _gcr_base[R_gcr_cl + O_gcr_other] = core;
    Mem::sync();
  }

  void run_other_vp(unsigned vp)
  { _gcr_base[R_cpc_co + O_cpc_vp_run] = vp; }

  void stop_other_vp(unsigned vp)
  { _gcr_base[R_cpc_co + O_cpc_vp_stop] = vp; }

  Unsigned32 other_running() const
  { return _gcr_base[R_cpc_co + O_cpc_vp_running]; }
};

// --------------------------------------------------
IMPLEMENTATION:

#include <cstdio>
#include "boot_alloc.h"
#include "panic.h"
#include "mem_layout.h"

Cm *Cm::cm;

PUBLIC static inline
bool
Cm::present()
{
  return Mips::Configs::read().r<3>().cmgcr();
}

PRIVATE template<unsigned REG_WIDTH>
void
Cm_x<REG_WIDTH>::setup_cpc() override
{
  // if CPC is not available return
  if (!(_gcr_base[R_gcr_cpc_status] & 1))
    return;

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
  set_cl_coherence(0xff);

  // paranoia check that the current core enters
  // coherent execution mode
  for (unsigned i = 0; i < 100; ++i)
    {
      asm volatile ("ehb" : : : "memory");
      Mem::sync();
      auto s = get_stat_conf();
      if (s.seq_state() == 7)
        return;
    }

  printf("MIPS: warning boot core did not reach U6 state.\n");
}

PUBLIC template<unsigned REG_WIDTH>
Cm_x<REG_WIDTH>::Cm_x(unsigned rev, Phys_mem_addr gcr_phys, Address gcr_base)
: Cm(rev, gcr_phys), _gcr_base(gcr_base)
{
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
}

PUBLIC static
void
Cm::init()
{
  if (!Cm::present())
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

  auto gcr_phys = Phys_mem_addr(v);

  Register_block<32> _gcrs(Mem_layout::ioremap_nocache(v << 4, 0x8000));

  printf("MIPS: Coherency Manager (CM) found: phys=%08lx(<<4) virt=%08lx\n",
         v, _gcrs.get_mmio_base());

  unsigned raw_rev = _gcrs[Cm::R_gcr_rev];
  unsigned rev = raw_rev >> 8;
  printf("MIPS: CM: revision %s (%08x)\n",
         (rev == Rev_cm2) ? "2.0" : (rev == Rev_cm2_5) ? "2.5" :
         (rev == Rev_cm3) ? "3.0" : (rev == Rev_cm3_5) ? "3.5" :
         "<unknown>", raw_rev);

  switch (rev)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case Rev_cm2:
    case Rev_cm2_5:
      cm = new Boot_object<Cm2>(rev, gcr_phys, _gcrs.get_mmio_base());
      break;

    case Rev_cm3:
    case Rev_cm3_5:
      cm = new Boot_object<Cm3>(rev, gcr_phys, _gcrs.get_mmio_base());
      break;

    default:
      printf("MIPS: CM: unknown CM revision, disable CM\n");
      return;
    }

  cm->setup_cpc();
}
