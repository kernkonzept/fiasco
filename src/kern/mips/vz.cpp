INTERFACE:

#include "entry_frame.h"
#include "per_cpu_data.h"
#include "cpu.h"
#include "timer.h" // assumes timer-mips CP0 timer

class Context;

class Vz
{
public:
  struct Options
  {
    unsigned ctl_0_ext:1;
    unsigned ctl_1    :1;
    unsigned ctl_2    :1;
  };

  struct Owner
  {
    Context *ctxt = 0;
    int guest_id = -1;
  };

  static Per_cpu<Owner> owner;

  struct State
  {
    Mword version;
    Mword size;

    Mword ctl_0_ext;
    Mword ctl_gtoffset;
    Mword ctl_1; ///< not exported, probably needed for EID
    Mword ctl_2;
    // Mword ctl_3; ///< currently no shadow sets are exported

    /**
     * Dirty cp0 register map..
     * On VM exit each bit corresponds to an updated register.
     * On VM entry each 1 bit means state that was changed by the VMM
     * and needs to be loaded into HW.
     */
    Unsigned32 current_cp0_map;
    Unsigned32 modified_cp0_map;

    // Status and Ctl0 are always saved on exit
    Mword ctl_0;
    Mword g_status;      // $12, 0

    // The timestamp and the cause are always saved together
    Unsigned64 _saved_cause_timestamp;
    Mword g_cause;       // $13, 0
    Mword g_compare;     // $11, 0

    Mword g_cfg[6];      // Guest.Config[0..5]

    Mword g_index;       // $0, 0
    Mword g_entry_lo[2]; // $2 and $3
    Mword g_context;     // $4, 0
    Mword g_page_mask;   // $5, 0
    Mword g_wired;       // $6, 0
    Mword g_entry_hi;    // $10, 0

    Mword g_page_grain;  // $5, 1
    Mword g_seg_ctl[3];  // $5, 2 .. 4

    Mword g_pw_base;     // $5, 5
    Mword g_pw_field;    // $5, 6
    Mword g_pw_size;     // $5, 7
    Mword g_pw_ctl;      // $6, 6

    Mword g_intctl;      // $12, 1
    Mword g_ulr;         // $4, 2
    Mword g_epc;         // $14, 0
    Mword g_error_epc;   // $30, 0
    Mword g_ebase;       // $15, 1

    Mword g_hwrena;      // $7, 0
    Mword g_bad_v_addr;  // $8, 0
    Mword g_bad_instr;   // $8, 1
    Mword g_bad_instr_p; // $8, 2

    Mword g_kscr[8];     // $31, 0 .. 7


    struct Tlb_entry
    {
      Mword mask;
      Mword entry_hi;
      Mword entry_lo[2];
    };

    enum { Max_guest_wired = 16 };
    Tlb_entry g_tlb_wired[Max_guest_wired];

    enum Modified_bits
    {
      M_cfg       = 1UL <<  0, ///< Guest.Config registers
      M_mmu       = 1UL <<  1, ///< MMU registers
      M_xlat      = 1UL <<  2, ///< Address translation registers
      M_pw        = 1UL <<  3, ///< Page walker registers
      M_bad       = 1UL <<  4, ///< BadVAddr, BadInstr, BadInstrP
      M_kscr      = 1UL <<  5, ///< KScr0..7
      M_status    = 1UL <<  6, ///< Status
      M_cause     = 1UL <<  7, ///< Cause
      M_epc       = 1UL <<  8, ///< EPC, ErrorEPC
      M_hwrena    = 1UL <<  9, ///< HWRena
      M_intctl    = 1UL << 10, ///< IntCtl
      M_ebase     = 1UL << 11, ///< EBase
      M_ulr       = 1UL << 12, ///< ULR
      M_ctl_0     = 1UL << 16, ///< GuestCtl0
      M_ctl_0_ext = 1UL << 17, ///< GuestCtl0Ext
      M_ctl_2     = 1UL << 18, ///< GuestCtl2
      M_gtoffset  = 1UL << 24, ///< GTOffset
      M_compare   = 1UL << 25, ///< GuestCompare
      M_llbit     = 1UL << 26, ///< when set, clear LLBit in LLAddr register
    };

    void init();
    void load_ctl() const;
    void save_ctl();

    void mfg_kscr(unsigned kscr_n, unsigned x);
    void mtg_kscr(unsigned kscr_n, unsigned x) const;
    void mtg_cfg(unsigned x) const;

    void save_full(int guest_id);
    void load_full(int guest_id);
    void load_selective(int guest_id);
    void save_on_exit(Entry_frame::Cause cause);
    void update_cause_ti();

  private:
    void load_cause();
    void load_guest_tlb_entry(int guest_id, unsigned i);
    void save_guest_tlb_entry(int guest_id, unsigned i);

    enum
    {
      Ctl_0_mbz  = 0x7f7f0281, ///< must be zero mask CTL0
      Ctl_0_mb1  = 0x0c000000, ///< must be one bits CTL0
      Ctl_0_dflz = Ctl_0_mbz & 0x1c7f0280, ///< default zero mask CTL0
      Ctl_0_dfl1 = Ctl_0_mb1 | 0x1c000000, ///< default one bits CTL0

      Ctl_0_ext_mbz = 0x0d7,
      Ctl_0_ext_mb1 = 0x000,
      Ctl_0_ext_dflz = Ctl_0_ext_mbz & 0x0c0,
      Ctl_0_ext_dfl1 = Ctl_0_ext_mb1 | 0x000,

      Ctl_2_mbz = 0xfc00,
    };
  };

  static Mips::Configs const *guest_config()
  { return &guest_cfg; }

  static Mips::Configs const *guest_config_write()
  { return &guest_cfg_write; }

private:
  static Mips::Configs guest_cfg;
  static Mips::Configs guest_cfg_write;

  static Options options;
};

// ------------------------------------------------------------
IMPLEMENTATION:

#include "cpu.h"
#include "static_init.h"
#include "processor.h"
#include <cstdio>
#include <cstring>

DEFINE_PER_CPU Per_cpu<Vz::Owner> Vz::owner;
Mips::Configs Vz::guest_cfg;
Mips::Configs Vz::guest_cfg_write;
Vz::Options Vz::options;

STATIC_INITIALIZE(Vz);


IMPLEMENT inline ALWAYS_INLINE
void
Vz::State::mfg_kscr(unsigned kscr_n, unsigned x)
{
  if (kscr_n & (1 << x))
    Mips::mfgc0(&g_kscr[x], 31, x);
}

PRIVATE static inline ALWAYS_INLINE
void
Vz::mfg_cfg_init(unsigned x)
{
  if ((x == 0) || guest_cfg.c[x - 1].m())
    guest_cfg.c[x]._v = Mips::mfgc0_32(16, x);
}



IMPLEMENT inline ALWAYS_INLINE
void
Vz::State::mtg_kscr(unsigned kscr_n, unsigned x) const
{
  if (kscr_n & (1 << x))
    Mips::mtgc0(g_kscr[x], 31, x);
}

IMPLEMENT inline ALWAYS_INLINE
void
Vz::State::mtg_cfg(unsigned x) const
{
  if ((x == 0) || guest_cfg.c[x - 1].m())
    {
      Mword w_mask = guest_cfg_write.c[x]._v;
      Mword v = access_once(&g_cfg[x]);
      v = (v & w_mask) | (guest_cfg.c[x]._v & ~w_mask);
      Mips::mtgc0_32(v, 16, x);
    }
}

PUBLIC static
void
Vz::init()
{
  if (!Cpu::options.vz())
    return;

  printf("MIPS: enable VZ\n");

  // read initial Guest.Config registers
  mfg_cfg_init(0);

  if (Cpu::options.ftlb())
    {
      // try to enable FTLB / VTLB in the guest too
      if (guest_cfg.r<0>().mt() == 1)
        {
          auto gc0 = guest_cfg.r<0>();
          gc0.mt() = 4; // try dual TLB
          Mips::mtgc0_32(gc0._v, Mips::Cp0_config_0);
          Mips::ehb();
          mfg_cfg_init(0);

          if (guest_cfg.r<0>().mt() != 4)
            printf("MIPS: no VTLB/FTLB in guest supported\n");
        }
    }

  mfg_cfg_init(1);
  mfg_cfg_init(2);
  mfg_cfg_init(3);
  mfg_cfg_init(4);
  mfg_cfg_init(5);

#define ALLOW_IF_PRESENT(reg, f) \
  guest_cfg_write.r<reg>().f() = (Unsigned32)guest_cfg.r<reg>().f()

#define ALLOW_WRITE(reg, f) \
  guest_cfg_write.r<reg>().f() = (Mips::Cfg<reg>::f ## _bfm_t::Bits_type)~0UL

#define FORCE(reg, f, val)  guest_cfg.r<reg>().f() = val

  ALLOW_WRITE(0, k0);
  guest_cfg.r<0>().k0() = Mips::Cfg<0>::read().k0();
  ALLOW_WRITE(0, impl);
  ALLOW_WRITE(0, ku);
  ALLOW_WRITE(0, k23);

  FORCE(1, c2, 0); // cop2 not allowed for guest
  FORCE(1, md, 0); // MDMX not allowed (unknown feature)
  // disable watch registers, we need support for context switching WatchHi and
  // WatchLo registers: ALLOW(1, wr);
  FORCE(1, wr, 0);

  ALLOW_IF_PRESENT(1, pc);
  ALLOW_IF_PRESENT(1, ca);
  ALLOW_IF_PRESENT(1, fp);

  ALLOW_WRITE(2, su);
  ALLOW_WRITE(2, tu);

  // disable MSA as long as fiasco support is missing
  FORCE(3, msap, 0);
  // disable big page support (needs 64bit versions of some cp0 regs)
  FORCE(3, bpg, 0);
  // disable DSP ASE as long as fiasco does not support it
  FORCE(3, dspp, 0);
  FORCE(3, dsp2p, 0);
  FORCE(3, ctxtc, 0);  // disable context config for now
  FORCE(3, itl, 0);    // disable IFlowTrace for now
  FORCE(3, lpa, 0);    // disable LPA for now
  FORCE(3, veic, 0);   // disable VEIC (must be supported in fiasco)
  FORCE(3, mt, 0);     // disable guest MT
  FORCE(3, sm, 0);     // disable smart MIPS
  FORCE(3, tl, 0);     // disable trace logic;

  ALLOW_WRITE(3, isa_on_exc);
  ALLOW_IF_PRESENT(3, vint);
  ALLOW_IF_PRESENT(3, sp);
  ALLOW_IF_PRESENT(3, cdmm);
  ALLOW_IF_PRESENT(3, ulri);

  ALLOW_WRITE(4, ftlb_page_size2); // allow FTLB page size settings

  FORCE(5, msa_en, 0);

  ALLOW_WRITE(5, ufr);
  ALLOW_WRITE(5, sbri);
  ALLOW_WRITE(5, ufe);
  ALLOW_WRITE(5, fre);
  ALLOW_WRITE(5, cv);
  ALLOW_WRITE(5, k);

  ALLOW_IF_PRESENT(5, mrp);

#undef ALLOW_IF_PRESENT
#undef ALLOW_WRITE
#undef FORCE

  auto ctl0 = Mips::mfc0_32(Mips::Cp0_guest_ctl_0);
  if (ctl0 & (1UL << 19))
    options.ctl_0_ext = 1;

  if (ctl0 & (1UL << 22))
    options.ctl_1 = 1;

  if (ctl0 & (1UL << 7))
    options.ctl_2 = 1;
}

IMPLEMENT void
Vz::State::init()
{
  memset(this, 0, sizeof(State));

  version = 2;
  size = sizeof(State);

  Unsigned32 _ctl_0 = Mips::mfc0_32(Mips::Cp0_guest_ctl_0);

  if (_ctl_0 & (1UL << 19))
    ctl_0_ext = (Mips::mfc0_32(Mips::Cp0_guest_ctl_0_ext) & Ctl_0_ext_dflz)
                | Ctl_0_ext_dfl1;
  else
    ctl_0_ext = 0;

  ctl_0 = (_ctl_0 & Ctl_0_dflz) | Ctl_0_dfl1;

  for (unsigned i = 0; i < 6; ++i)
    g_cfg[i] = Vz::guest_cfg.c[i]._v;

  // everything is up-to-date and marked as modified so it will
  // be loaded upon VMM -> VM switch
  current_cp0_map = (Unsigned32)~0;
  modified_cp0_map = (Unsigned32)~0;
}

IMPLEMENT inline NEEDS[<cstdio>]
void
Vz::State::save_guest_tlb_entry(int guest_id, unsigned i)
{
  using namespace Mips;
  auto &tlb = g_tlb_wired[i];
  mtgc0_32(i, Cp0_index);
  ehb();
  asm volatile (".set push; .set virt; tlbgr; .set pop");
  ehb();
  auto ctl1 = Mips::mfc0_32(Mips::Cp0_guest_ctl_1);

  if ((unsigned)guest_id == ((ctl1 >> 16) & 0xff))
    {
      // no BPA supported in the guest, so far
      mfgc0_32(&tlb.mask, Cp0_page_mask);
      mfgc0(&tlb.entry_hi, Cp0_entry_hi);
      mfgc0(&tlb.entry_lo[0], Cp0_entry_lo1);
      mfgc0(&tlb.entry_lo[1], Cp0_entry_lo2);
    }
  else
    tlb.entry_hi = 1UL << 10; // EHINV

  if (0)
    printf("VZ[%p|%d]: saved TLB[%u] (%s) mask=%lx hi=%lx lo=%lx|%lx\n",
           this, guest_id, i, (tlb.entry_hi & (1UL << 10)) ? "inv" : "ok",
           tlb.mask, tlb.entry_hi, tlb.entry_lo[0],
           tlb.entry_lo[1]);
}


IMPLEMENT
void
Vz::State::save_full(int guest_id)
{
  using namespace Mips;
  auto c_map = current_cp0_map;
  write_now(&current_cp0_map, (Unsigned32)~0);

  if (EXPECT_TRUE(!(c_map & M_ctl_0)))
    save_ctl();

  if (EXPECT_TRUE(!(c_map & M_status)))
    mfgc0_32(&g_status, Mips::Cp0_status);

  // 12, 2 .. 3 SRSxxx not implemented (disabled in guest_config)
  if (EXPECT_TRUE(!(c_map & M_cause)))
    {
      // need the timestamp when we save the cause register
      _saved_cause_timestamp = Timer::get_current_counter();
      // alex: not sure if this is needed or if reads from cp0 are ordered
      ehb();
      mfgc0_32(&g_cause, Cp0_cause);
    }

  if (EXPECT_TRUE(!(c_map & M_compare)))
    mfgc0_32(&g_compare, Cp0_compare);

  if (EXPECT_TRUE(!(c_map & M_mmu)))
    {
      mfgc0_32(&g_index, Cp0_index);
      mfgc0(&g_entry_lo[0], Cp0_entry_lo1);
      mfgc0(&g_entry_lo[1], Cp0_entry_lo2);
      mfgc0(&g_context, Cp0_context);
      // 4, 3 XContextConfig not supported
      mfgc0_32(&g_page_mask, Cp0_page_mask);
      Mword w;
      mfgc0_32(&w, Cp0_wired);
      g_wired = w;
      mfgc0(&g_entry_hi, Cp0_entry_hi);

      w &= 0xff;
      if (EXPECT_FALSE(w > Max_guest_wired))
        w = Max_guest_wired;

      if (0 && w)
        printf("VZ[%p]: guest=%d save wired: %lu\n", this, guest_id, w);

      for (unsigned i = 0; i < w; ++i)
        save_guest_tlb_entry(guest_id, i);
    }

  // 4, 1 ContextConfig not supported
  if (EXPECT_TRUE(!(c_map & M_xlat)))
    {
      mfgc0_32(&g_page_grain, Cp0_page_grain);

      if (guest_cfg.r<3>().sc())
        {
          mfgc0(&g_seg_ctl[0], Cp0_seg_ctl_0);
          mfgc0(&g_seg_ctl[1], Cp0_seg_ctl_1);
          mfgc0(&g_seg_ctl[2], Cp0_seg_ctl_2);
        }
    }

  if (EXPECT_TRUE(!(c_map & M_pw)) && guest_cfg.r<3>().pw())
    {
      mfgc0(&g_pw_base, Cp0_pw_base);
      mfgc0(&g_pw_field, Cp0_pw_field);
      mfgc0(&g_pw_size, Cp0_pw_size);
      mfgc0_32(&g_pw_ctl, Cp0_pw_ctl);
    }

  if (EXPECT_TRUE(!(c_map & M_intctl)))
    mfgc0_32(&g_intctl, Cp0_int_ctl);

  if (EXPECT_TRUE(!(c_map & M_ulr)) && guest_cfg.r<3>().ulri())
    mfgc0(&g_ulr, Cp0_user_local);


  // 13, 5 NestedExc not supported in guest
  if (EXPECT_TRUE(!(c_map & M_epc)))
    {
      mfgc0(&g_epc, Cp0_epc);
      mfgc0(&g_error_epc, Cp0_err_epc);
    }

  if (EXPECT_TRUE(!(c_map & M_ebase)))
    mfgc0(&g_ebase, Cp0_ebase);


  if (EXPECT_TRUE(!(c_map & M_hwrena)))
    mfgc0_32(&g_hwrena, Cp0_hw_rena);

  if (EXPECT_TRUE(!(c_map & M_bad)))
    {
      mfgc0(&g_bad_v_addr, Cp0_bad_v_addr);

      if (guest_cfg.r<3>().bi())
        mfgc0_32(&g_bad_instr, Cp0_bad_instr);

      if (guest_cfg.r<3>().bp())
        mfgc0_32(&g_bad_instr_p, Cp0_bad_instr_p);
    }

  // FIXME: Think about Tag and Data registers

  auto kscr_n = guest_cfg.r<4>().k_scr_num();
  if (EXPECT_TRUE(!(c_map & M_kscr)))
    {
      mfg_kscr(kscr_n, 2);
      mfg_kscr(kscr_n, 3);
      mfg_kscr(kscr_n, 4);
      mfg_kscr(kscr_n, 5);
      mfg_kscr(kscr_n, 6);
      mfg_kscr(kscr_n, 7);
    }
}

IMPLEMENT inline NEEDS["timer.h"]
void
Vz::State::update_cause_ti()
{
  enum { Cause_TI = 1UL << 30 };

  if (g_cause & Cause_TI)
    return;

  Unsigned64 ct = Timer::get_current_counter();
  Unsigned64 gc = ct + (Signed32) ctl_gtoffset;
  Unsigned64 last_gc = _saved_cause_timestamp + (Signed32) ctl_gtoffset;
  Unsigned64 gcomp = ((Unsigned32) g_compare) | (last_gc & 0xffffffff00000000);

  if (gcomp < last_gc)
    gcomp += 0x100000000;

  if (gcomp <= gc)
    g_cause |= Cause_TI;
}

IMPLEMENT inline NEEDS["timer.h"]
void
Vz::State::load_cause()
{
  using namespace Mips;
  enum { Cause_TI = 1UL << 30 };

  mtgc0_32(g_cause, Cp0_cause);
  if (g_cause & Cause_TI)
    return;

  // make sure Guest.Cause is written before we read the counter
  // Note, this additionally ensures guest compare is written before we
  // read it below.
  ehb();
  Unsigned64 ct = Timer::get_current_counter();
  Unsigned64 gc = ct + (Signed32) ctl_gtoffset;
  Unsigned64 last_gc = _saved_cause_timestamp + (Signed32) ctl_gtoffset;

  Unsigned32 gcmp = mfgc0_32(Cp0_compare);
  Unsigned64 gcomp = gcmp | (last_gc & 0xffffffff00000000);

  if (gcomp < last_gc)
    gcomp += 0x100000000;

  if (gcomp <= gc)
    {
      g_cause |= Cause_TI;
      mtgc0_32(g_cause, Cp0_cause);
    }
}

IMPLEMENT inline
void
Vz::State::load_guest_tlb_entry(int guest_id, unsigned i)
{
  using namespace Mips;
  auto const &tlb = g_tlb_wired[i];
  if (0)
    printf("VZ[%p|%d]: load TLB[%u] (%s) mask=%lx hi=%lx lo=%lx|%lx\n",
           this, guest_id, i, (tlb.entry_hi & (1UL << 10)) ? "inv" : "ok",
           tlb.mask, tlb.entry_hi, tlb.entry_lo[0],
           tlb.entry_lo[1]);

  mtgc0_32(i, Cp0_index);
  mtgc0_32(tlb.mask, Cp0_page_mask);
  mtgc0(tlb.entry_hi, Cp0_entry_hi);
  mtgc0(tlb.entry_lo[0], Cp0_entry_lo1);
  mtgc0(tlb.entry_lo[1], Cp0_entry_lo2);
  asm volatile (
      ".set push\n\t"
      ".set noat\n\t"
      "  mfc0\t$1, $10, 4\n\t"
      "  ins \t$1, %0, 16, 8\n\t"
      "  mtc0\t$1, $10, 4\n\t"
      ".set pop" : : "r"(guest_id));
  ehb();
  asm volatile (".set push; .set virt; tlbgwi; .set pop");
  ehb();
}


IMPLEMENT
void
Vz::State::load_full(int guest_id)
{
  using namespace Mips;
  write_now(&modified_cp0_map, 0);

  load_ctl();

  mtgc0_32(g_status, Mips::Cp0_status);
  mtgc0_32(g_compare, Cp0_compare); // Compare (must be loaded before load_cause)
  load_cause();

  mtg_cfg(0);
  mtg_cfg(1);
  mtg_cfg(2);
  mtg_cfg(3);
  mtg_cfg(4);
  mtg_cfg(5);


  unsigned w = access_once(&g_wired);
  mtgc0_32(w, Cp0_wired);

  w &= 0xff;
  if (EXPECT_FALSE(w > Max_guest_wired))
    w = Max_guest_wired;

  if (0 && w)
    printf("VZ[%p]: guest=%d load wired: %u\n", this, guest_id, w);

  for (unsigned i = 0; i < w; ++i)
    load_guest_tlb_entry(guest_id, i);

  mtgc0_32(g_index, Cp0_index);
  mtgc0(g_entry_lo[0], Cp0_entry_lo1);
  mtgc0(g_entry_lo[1], Cp0_entry_lo2);
  mtgc0(g_context, Cp0_context);
  mtgc0_32(g_page_mask, Cp0_page_mask);
  mtgc0(g_entry_hi, Cp0_entry_hi);

  mtgc0_32(g_page_grain, Cp0_page_grain);

  if (guest_cfg.r<3>().sc())
    {
      mtgc0(g_seg_ctl[0], Cp0_seg_ctl_0);
      mtgc0(g_seg_ctl[1], Cp0_seg_ctl_1);
      mtgc0(g_seg_ctl[2], Cp0_seg_ctl_2);
    }

  // 4, 3 XContextConfig not supported
  if (guest_cfg.r<3>().pw())
    {
      mtgc0(g_pw_base, Cp0_pw_base);
      mtgc0(g_pw_field, Cp0_pw_field);
      mtgc0(g_pw_size, Cp0_pw_size);
      mtgc0_32(g_pw_ctl, Cp0_pw_ctl);
    }

  mtgc0_32(g_intctl, Cp0_int_ctl);
  if (guest_cfg.r<3>().ulri())
    mtgc0(g_ulr, Cp0_user_local);

  // 12, 2 .. 3 SRSxxx not implemented (disabled in guest_config)
  // 13, 5 NestedExc not supported in guest
  mtgc0(g_epc, Cp0_epc);
  mtgc0(g_error_epc, Cp0_err_epc);
  mtgc0(g_ebase, Cp0_ebase);

  mtgc0_32(g_hwrena, Cp0_hw_rena);
  mtgc0(g_bad_v_addr, Cp0_bad_v_addr);

  if (guest_cfg.r<3>().bi())
    mtgc0_32(g_bad_instr, Cp0_bad_instr); // BadInstr

  if (guest_cfg.r<3>().bp())
    mtgc0_32(g_bad_instr_p, Cp0_bad_instr_p); // BadInstrP

  if (guest_cfg.r<5>().llb())
    mtgc0(0, Cp0_load_linked_addr);

  // FIXME: Think about Tag and Data registers

  auto kscr_n = guest_cfg.r<4>().k_scr_num();
  mtg_kscr(kscr_n, 2);
  mtg_kscr(kscr_n, 3);
  mtg_kscr(kscr_n, 4);
  mtg_kscr(kscr_n, 5);
  mtg_kscr(kscr_n, 6);
  mtg_kscr(kscr_n, 7);
}

IMPLEMENT
void
Vz::State::load_selective(int guest_id)
{
  using namespace Mips;
  auto mod_map = modified_cp0_map;
  write_now(&modified_cp0_map, 0);

  if (EXPECT_FALSE(mod_map & M_ctl_0))
    {
      auto c0 = access_once(&ctl_0);
      c0 &= Ctl_0_mbz;
      c0 |= Ctl_0_mb1;
      mtc0_32(c0, Cp0_guest_ctl_0);
    }

  if (EXPECT_FALSE(mod_map & M_status))
    mtgc0_32(g_status, Mips::Cp0_status);

  if (EXPECT_FALSE(mod_map & M_gtoffset))
    mtc0_32(ctl_gtoffset, Cp0_gt_offset);

  if (EXPECT_FALSE(mod_map & M_compare))
    mtgc0_32(g_compare, Cp0_compare); // Compare (must be loaded before load_cause)

  if (EXPECT_FALSE(mod_map & M_cause))
    load_cause();


  if (EXPECT_FALSE(mod_map & M_ctl_0_ext) && Vz::options.ctl_0_ext)
    {
      auto c0 = access_once(&ctl_0_ext);
      c0 &= Ctl_0_ext_mbz;
      c0 |= Ctl_0_ext_mb1;
      mtc0_32(c0, Cp0_guest_ctl_0_ext);
    }

  if (EXPECT_FALSE(mod_map & M_ctl_2) && Vz::options.ctl_2)
    {
      auto c2 = access_once(&ctl_2);
      c2 &= Ctl_2_mbz;
      mtc0_32(c2, Cp0_guest_ctl_2);
    }

  // Guest config
  if (EXPECT_FALSE(mod_map & M_cfg))
    {
      mtg_cfg(0);
      mtg_cfg(1);
      mtg_cfg(2);
      mtg_cfg(3);
      mtg_cfg(4);
      mtg_cfg(5);
    }

  // Guest MMU
  if (EXPECT_FALSE(mod_map & M_mmu))
    {
      unsigned w = access_once(&g_wired);
      mtgc0_32(w, Cp0_wired);

      w &= 0xff;
      if (EXPECT_FALSE(w > Max_guest_wired))
        w = Max_guest_wired;

      if (0 && w)
        printf("VZ[%p]: guest=%d load wired: %u (sel)\n", this, guest_id, w);

      for (unsigned i = 0; i < w; ++i)
        load_guest_tlb_entry(guest_id, i);

      mtgc0_32(g_index, Cp0_index);
      mtgc0(g_entry_lo[0], Cp0_entry_lo1);
      mtgc0(g_entry_lo[1], Cp0_entry_lo2);
      mtgc0(g_context, Cp0_context);
      mtgc0_32(g_page_mask, Cp0_page_mask);
      mtgc0(g_entry_hi, Cp0_entry_hi);
    }

  // Address Translation segmentation
  if (EXPECT_FALSE(mod_map & M_xlat))
    {
      mtgc0_32(g_page_grain, Cp0_page_grain);

      if (guest_cfg.r<3>().sc())
        {
          mtgc0(g_seg_ctl[0], Cp0_seg_ctl_0);
          mtgc0(g_seg_ctl[1], Cp0_seg_ctl_1);
          mtgc0(g_seg_ctl[2], Cp0_seg_ctl_2);
        }
    }

  if (EXPECT_FALSE(mod_map & M_pw)
      && guest_cfg.r<3>().pw())
    {
      mtgc0(g_pw_base, Cp0_pw_base);
      mtgc0(g_pw_field, Cp0_pw_field);
      mtgc0(g_pw_size, Cp0_pw_size);
      mtgc0_32(g_pw_ctl, Cp0_pw_ctl);
    }

  if (EXPECT_FALSE(mod_map & M_intctl))
    mtgc0_32(g_intctl, Cp0_int_ctl);

  if (EXPECT_FALSE(mod_map & M_ulr)
      && guest_cfg.r<3>().ulri())
    mtgc0(g_ulr, Cp0_user_local);

  if (EXPECT_FALSE(mod_map & M_epc))
    {
      mtgc0(g_epc, Cp0_epc);
      mtgc0(g_error_epc, Cp0_err_epc);
    }

  if (EXPECT_FALSE(mod_map & M_ebase))
    mtgc0(g_ebase, Cp0_ebase);

  if (EXPECT_FALSE(mod_map & M_hwrena))
    mtgc0_32(g_hwrena, Cp0_hw_rena);

  if (EXPECT_FALSE(mod_map & M_bad))
    {
      mtgc0(g_bad_v_addr, Cp0_bad_v_addr);

      if (guest_cfg.r<3>().bi())
        mtgc0_32(g_bad_instr, Cp0_bad_instr);

      if (guest_cfg.r<3>().bp())
        mtgc0_32(g_bad_instr_p, Cp0_bad_instr_p);
    }

  if (EXPECT_FALSE(mod_map & M_llbit))
    if (guest_cfg.r<5>().llb())
      mtgc0(0, Cp0_load_linked_addr);

  // 12, 2 .. 3 SRSxxx not implemented (disabled in guest_config)
  // 13, 5 NestedExc not supported in guest
  // FIXME: Think about Tag and Data registers

  auto kscr_n = guest_cfg.r<4>().k_scr_num();
  if (EXPECT_FALSE(mod_map & M_kscr))
    {
      mtg_kscr(kscr_n, 2);
      mtg_kscr(kscr_n, 3);
      mtg_kscr(kscr_n, 4);
      mtg_kscr(kscr_n, 5);
      mtg_kscr(kscr_n, 6);
      mtg_kscr(kscr_n, 7);
    }
}

IMPLEMENT inline
void
Vz::State::save_on_exit(Entry_frame::Cause)
{
  auto c_map = current_cp0_map;
  if (!(c_map & M_ctl_0))
    save_ctl();   // save GuestCtl0

  if (!(c_map & M_status))
    Mips::mfgc0_32(&g_status, Mips::Cp0_status);

  write_now(&current_cp0_map, c_map | M_ctl_0 | M_status);
}

IMPLEMENT inline
void
Vz::State::load_ctl() const
{
  using namespace Mips;

  auto c0 = access_once(&ctl_0);
  c0 &= Ctl_0_mbz;
  c0 |= Ctl_0_mb1;
  mtc0_32(c0, Cp0_guest_ctl_0);
  mtc0_32(ctl_gtoffset, Cp0_gt_offset);

  if (Vz::options.ctl_0_ext)
    {
      auto c0 = access_once(&ctl_0_ext);
      c0 &= Ctl_0_ext_mbz;
      c0 |= Ctl_0_ext_mb1;
      mtc0_32(c0, Cp0_guest_ctl_0_ext);
    }

  if (Vz::options.ctl_2)
    {
      auto c2 = access_once(&ctl_2);
      c2 &= Ctl_2_mbz;
      mtc0_32(c2, Cp0_guest_ctl_2);
    }
}

IMPLEMENT inline
void
Vz::State::save_ctl()
{
  ctl_0 = Mips::mfc0_32(Mips::Cp0_guest_ctl_0);
}


