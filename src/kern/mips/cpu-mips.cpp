INTERFACE [mips]:

#include "per_cpu_data.h"
#include "initcalls.h"
#include "processor.h"
#include "types.h"
#include <cxx/bitfield>

namespace Mips {
  struct Cfg_base
  {
    Unsigned32 _v;
    Cfg_base() = default;
    Cfg_base(Unsigned32 v) : _v(v) {}
    CXX_BITFIELD_MEMBER(31, 31, m, _v);
  };

  template<unsigned IDX> struct Cfg;
  template<> struct Cfg<0> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  2, k0, _v);
    CXX_BITFIELD_MEMBER( 3,  3, vi, _v);
    CXX_BITFIELD_MEMBER( 7,  9, mt, _v);
    CXX_BITFIELD_MEMBER(10, 12, ar, _v);
    CXX_BITFIELD_MEMBER(13, 14, at, _v);
    CXX_BITFIELD_MEMBER(15, 15, be, _v);
    CXX_BITFIELD_MEMBER(16, 24, impl, _v);
    CXX_BITFIELD_MEMBER(25, 27, ku, _v);
    CXX_BITFIELD_MEMBER(28, 30, k23, _v);
    static Cfg<0> read() { return mfc0_32(16, 0); }
  };

  template<> struct Cfg<1> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  0, fp, _v);
    CXX_BITFIELD_MEMBER( 1,  1, ep, _v);
    CXX_BITFIELD_MEMBER( 2,  2, ca, _v);
    CXX_BITFIELD_MEMBER( 3,  3, wr, _v);
    CXX_BITFIELD_MEMBER( 4,  4, pc, _v);
    CXX_BITFIELD_MEMBER( 5,  5, md, _v);
    CXX_BITFIELD_MEMBER( 6,  6, c2, _v);
    CXX_BITFIELD_MEMBER( 7,  9, da, _v);
    CXX_BITFIELD_MEMBER(10, 12, dl, _v);
    CXX_BITFIELD_MEMBER(13, 15, ds, _v);
    CXX_BITFIELD_MEMBER(16, 18, ia, _v);
    CXX_BITFIELD_MEMBER(19, 21, il, _v);
    CXX_BITFIELD_MEMBER(22, 24, is, _v);
    CXX_BITFIELD_MEMBER(25, 30, mmu_size, _v);
    static Cfg<1> read() { return mfc0_32(16, 1); }
  };

  template<> struct Cfg<2> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  3, sa, _v);
    CXX_BITFIELD_MEMBER( 4,  7, sl, _v);
    CXX_BITFIELD_MEMBER( 8, 11, ss, _v);
    CXX_BITFIELD_MEMBER(12, 15, su, _v);
    CXX_BITFIELD_MEMBER(16, 19, ta, _v);
    CXX_BITFIELD_MEMBER(20, 23, tl, _v);
    CXX_BITFIELD_MEMBER(24, 27, ts, _v);
    CXX_BITFIELD_MEMBER(28, 30, tu, _v);
    static Cfg<2> read() { return mfc0_32(16, 2); }
  };

  template<> struct Cfg<3> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  0, tl, _v);
    CXX_BITFIELD_MEMBER( 1,  1, sm, _v);
    CXX_BITFIELD_MEMBER( 2,  2, mt, _v);
    CXX_BITFIELD_MEMBER( 3,  3, cdmm, _v);
    CXX_BITFIELD_MEMBER( 4,  4, sp, _v);
    CXX_BITFIELD_MEMBER( 5,  5, vint, _v);
    CXX_BITFIELD_MEMBER( 6,  6, veic, _v);
    CXX_BITFIELD_MEMBER( 7,  7, lpa, _v);
    CXX_BITFIELD_MEMBER( 8,  8, itl, _v);
    CXX_BITFIELD_MEMBER( 9,  9, ctxtc, _v);
    CXX_BITFIELD_MEMBER(10, 10, dspp, _v);
    CXX_BITFIELD_MEMBER(11, 11, dsp2p, _v);
    CXX_BITFIELD_MEMBER(12, 12, rxi, _v);
    CXX_BITFIELD_MEMBER(13, 13, ulri, _v);
    CXX_BITFIELD_MEMBER(14, 15, isa, _v);
    CXX_BITFIELD_MEMBER(16, 16, isa_on_exc, _v);
    CXX_BITFIELD_MEMBER(17, 17, mcu, _v);
    CXX_BITFIELD_MEMBER(18, 20, mmar, _v);
    CXX_BITFIELD_MEMBER(21, 22, iplw, _v);
    CXX_BITFIELD_MEMBER(23, 23, vz, _v);
    CXX_BITFIELD_MEMBER(24, 24, pw, _v);
    CXX_BITFIELD_MEMBER(25, 25, sc, _v);
    CXX_BITFIELD_MEMBER(26, 26, bi, _v);
    CXX_BITFIELD_MEMBER(27, 27, bp, _v);
    CXX_BITFIELD_MEMBER(28, 28, msap, _v);
    CXX_BITFIELD_MEMBER(29, 29, cmgcr, _v);
    CXX_BITFIELD_MEMBER(30, 30, bpg, _v);
    static Cfg<3> read() { return mfc0_32(16, 3); }
  };

  template<> struct Cfg<4> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  7, mmu_sz_ext, _v);
    CXX_BITFIELD_MEMBER( 0,  3, ftlb_sets, _v);
    CXX_BITFIELD_MEMBER( 4,  7, ftlb_ways, _v);
    CXX_BITFIELD_MEMBER( 0,  7, ftlb_info, _v);
    CXX_BITFIELD_MEMBER( 8, 12, ftlb_page_size2, _v);
    CXX_BITFIELD_MEMBER( 8, 10, ftlb_page_size1, _v);
    CXX_BITFIELD_MEMBER(14, 15, mmu_ext_def, _v);
    CXX_BITFIELD_MEMBER(16, 23, k_scr_num, _v);
    CXX_BITFIELD_MEMBER(24, 27, vtlb_sz_ext, _v);
    CXX_BITFIELD_MEMBER(28, 28, ae, _v);
    CXX_BITFIELD_MEMBER(29, 30, ie, _v);
    static Cfg<4> read() { return mfc0_32(16, 4); }
  };

  template<> struct Cfg<5> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  0, nf_exists, _v);
    CXX_BITFIELD_MEMBER( 2,  2, ufr, _v);
    CXX_BITFIELD_MEMBER( 3,  3, mrp, _v);
    CXX_BITFIELD_MEMBER( 4,  4, llb, _v);
    CXX_BITFIELD_MEMBER( 5,  5, mvh, _v);
    CXX_BITFIELD_MEMBER( 6,  6, sbri, _v);
    CXX_BITFIELD_MEMBER( 7,  7, vp, _v);
    CXX_BITFIELD_MEMBER( 8,  8, fre, _v);
    CXX_BITFIELD_MEMBER( 9,  9, ufe, _v);
    CXX_BITFIELD_MEMBER(10, 10, l2c, _v);
    CXX_BITFIELD_MEMBER(11, 11, dec, _v);
    CXX_BITFIELD_MEMBER(13, 13, xnp, _v);
    CXX_BITFIELD_MEMBER(27, 27, msa_en, _v);
    CXX_BITFIELD_MEMBER(28, 28, eva, _v);
    CXX_BITFIELD_MEMBER(29, 29, cv, _v);
    CXX_BITFIELD_MEMBER(30, 30, k, _v);
    static Cfg<5> read() { return mfc0_32(16, 5); }
  };

  struct Configs
  {
    Cfg_base c[6];

    template<unsigned IDX> Cfg<IDX> r() const { return Cfg<IDX>(c[IDX]._v); }
    template<unsigned IDX> Cfg<IDX> &r() { return static_cast<Cfg<IDX>&>(c[IDX]); }

    template<unsigned IDX>
    void read_reg()
    {
      if (IDX == 0 || c[IDX - 1].m())
        c[IDX] = Cfg<IDX>::read();
      else
        c[IDX] = 0;
    }

    void read_all()
    {
      read_reg<0>();
      read_reg<1>();
      read_reg<2>();
      read_reg<3>();
      read_reg<4>();
      read_reg<5>();
    }

    static Configs read()
    {
      Configs c;
      c.read_all();
      return c;
    }
  };
}

/**
 * Define an entry in the MIPS CPU specific hooks table.
 * \param id_mask  32bit mask value to mask bits in the ProcId that shall
 *                 not be matched (cleared bits are cleared before the match).
 * \param id       32bit ID value that is compared against the masked ProcId
 *                 value.
 * \param hooks    Pointer to a Cpu::Hooks object that is called on a match.
 *
 * The table contains mask and id fields that are matched against
 * the Processor ID CP0 register and called if a match is found.
 */
#define DEFINE_MIPS_CPU_TYPE(id_mask, id, hooks) \
  static Cpu::Cpu_type const __attribute__((used, section(".mips.cpu_type"))) \
    _mips_cpu_type_##__COUNTER__ = { id_mask, id, hooks }

EXTENSION class Cpu
{
public:
  /**
   * Abstract hooks interface where hooks are called for
   * matching CPUs.
   */
  struct Hooks
  {
    virtual void init(Cpu_number, bool resume, Unsigned32 prid) = 0;
  };

  /// Entry in the CPU hooks table.
  struct Cpu_type
  {
    Unsigned32 id_mask;
    Unsigned32 id;
    Hooks *hooks;
  };

  void init(Cpu_number cpu, bool resume, bool is_boot_cpu = false);

  static Unsigned64 const _frequency
    = (Unsigned64)Config::Cpu_frequency * 1000000;

  static Unsigned64 frequency() { return _frequency; }
  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  Cpu(Cpu_number cpu) { set_id(cpu); }

  static Mword stack_align(Mword stack)
  { return stack & ~0x3; }

  Cpu_phys_id phys_id() const
  { return _phys_id; }

  static Unsigned64 rdtsc()
  { return (Unsigned32)(Mips::mfc0_32(9, 0)); }

  static unsigned phys_bits()
  {
    // FIXME: store phys bits on init and return those
    // FIXME: currently limiting phys bits to 48
    return MWORD_BITS <= 32 ? MWORD_BITS : 48;
  }

  static void debugctl_enable() {}
  static void debugctl_disable() {}
  static Unsigned32 get_scaler_tsc_to_ns() { return 0; }
  static Unsigned32 get_scaler_tsc_to_us() { return 0; }
  static Unsigned32 get_scaler_ns_to_tsc() { return 0; }

  struct Options
  {
    Mword _o;
    CXX_BITFIELD_MEMBER (0, 0, tlbinv, _o);
    CXX_BITFIELD_MEMBER (1, 1, ulr,    _o);
    CXX_BITFIELD_MEMBER (2, 2, vz,     _o);
    CXX_BITFIELD_MEMBER (3, 3, bi,     _o); /// < BadInstr supported
    CXX_BITFIELD_MEMBER (4, 4, bp,     _o); /// < BadInstrP supported
    CXX_BITFIELD_MEMBER (6, 6, ftlb,   _o); /// < Dual VTLB / FTLB found
    CXX_BITFIELD_MEMBER (7, 7, ftlbinv,_o); /// < Dual VTLB / FTLB full TLBINV
    CXX_BITFIELD_MEMBER (8, 8, segctl, _o); /// < Segmentation control
  };


  static unsigned tlb_size() { return _tlb_size; }
  static unsigned ftlb_sets() { return _ftlb_sets; }
  static unsigned ftlb_ways() { return _ftlb_ways; }
  static Options options;

private:
  friend struct Cpu_type;

  static Cpu_type _types[] __asm__("MIPS_cpu_types");
  static Cpu *_boot_cpu;
  static unsigned long _ns_per_cycle;
  static unsigned _tlb_size;
  static unsigned _ftlb_sets;
  static unsigned _ftlb_ways;
  static unsigned _default_cca;

  Cpu_phys_id _phys_id;

  void panic(char const *fmt, ...) const __attribute__((noreturn));
  void require(bool cond, char const *fmt, ...) const;
  void pr(char const *fmt, ...) const;
};

IMPLEMENTATION:

#include <cstdio>
#include "panic.h"
#include "cp0_status.h"
#include "alternatives.h"
#include "mem_layout.h"
#include "processor.h"

DEFINE_PER_CPU_P(0) Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
Cpu *Cpu::_boot_cpu;
unsigned long Cpu::_ns_per_cycle;
unsigned Cpu::_tlb_size;
unsigned Cpu::_ftlb_sets;
unsigned Cpu::_ftlb_ways;
unsigned Cpu::_default_cca;
Cpu::Options Cpu::options;

IMPLEMENT
void
Cpu::panic(char const *fmt, ...) const
{
  va_list list;
  va_start(list, fmt);
  printf("CPU[%d]: panic: ", cxx::int_value<Cpu_number>(id()));
  vprintf(fmt, list);
  va_end(list);
  panic("end");
}

IMPLEMENT
void
Cpu::require(bool cond, char const *fmt, ...) const
{
  if (cond)
    return;

  va_list list;
  va_start(list, fmt);
  printf("CPU[%d]: panic: ", cxx::int_value<Cpu_number>(id()));
  vprintf(fmt, list);
  va_end(list);
  panic("end");
}

IMPLEMENT
void
Cpu::pr(char const *fmt, ...) const
{
  va_list list;
  va_start(list, fmt);
  printf("CPU[%d]: ", cxx::int_value<Cpu_number>(id()));
  vprintf(fmt, list);
  va_end(list);
}

PUBLIC
bool
Cpu::if_show_infos() const
{ return id() == Cpu_number::boot_cpu() || !boot_cpu(); }

PUBLIC
void
Cpu::print_infos() const
{
  if (if_show_infos())
    pr("%lluMHz (%d TLBs) %s\n",
       frequency() / 1000000, tlb_size(),
       options.tlbinv() ? "TLBINV " : "");
}

PRIVATE inline NOEXPORT
void
Cpu::first_boot(bool is_boot_cpu)
{
  //AW: identify();

  auto c = Mips::Configs::read();
  if (c.r<0>().mt() != 1 && c.r<0>().mt() != 4)
    {
      char const *const tlb_type[] =
        {
          "None", "Standard TLB", "BAT", "Fixed Map",
          "Dual VTLB / FTLB", "<unk>", "<unk>", "<unk>"
        };
      panic("unsupported TLB type: %s (%d)\n", tlb_type[c.r<0>().mt()], c.r<0>().mt());
    }

  require(c.r<0>().vi() == 0, "virtual instruction caches not supported\n");
  require(c.r<0>().ar() > 0,  "MIPS r1 CPUs are notsupported\n");
  require(c.r<0>().m(), "CP0 Config1 register missing\n");

  require(c.r<1>().m(), "CP0 Config2 register missing\n");
  require(c.r<2>().m(), "CP0 Config3 register missing\n");

  unsigned tlb_size  = c.r<1>().mmu_size();
  unsigned ftlb_ps   = 0;
  unsigned ftlb_info = 0;

  Options opts = { 0 };

  opts.ulr() = c.r<3>().ulri();
  opts.vz()  = c.r<3>().vz();
  opts.bi()  = c.r<3>().bi();
  opts.bp()  = c.r<3>().bp();
  opts.segctl() = c.r<3>().sc();

  if (c.r<3>().m())
    {
      if (c.r<0>().ar() == 2 || c.r<4>().mmu_ext_def() == 3)
        {
          tlb_size |= (unsigned)c.r<4>().vtlb_sz_ext() << 6;
          ftlb_info = c.r<4>().ftlb_info();
          ftlb_ps = c.r<4>().ftlb_page_size2();
        }
      else if (c.r<4>().mmu_ext_def() == 2)
        {
          ftlb_info = c.r<4>().ftlb_info();
          ftlb_ps = c.r<4>().ftlb_page_size1();
        }
      else if (c.r<4>().mmu_ext_def() == 1)
        tlb_size |= (unsigned)c.r<4>().mmu_sz_ext() << 6;

      if (ftlb_info)
        {
          unsigned ps = 0;
          switch ((Mword)Config::PAGE_SIZE)
            {
            case 0x1000: // try to enable 4KB pages in FTLB
              ps = 1;
              break;
            case 0x4000: // try to enable 16KB pages in FTLB
              ps = 2;
              break;
            default:
              panic("FTLB: page size (0x%x) not supported with FTLB\n",
                    Config::PAGE_SIZE);
              break;
            }

          auto c4 = c.r<4>();
          c4.ftlb_page_size2() = ps;
          Mips::mtc0_32(c4._v, Mips::Cp0_config_4);
          Mips::ehb();
          c4._v = Mips::mfc0_32(Mips::Cp0_config_4);
          require(c4.ftlb_page_size2() == ps,
                  "FTLB: page size (0x%x) not supported in HW\n",
                  Config::PAGE_SIZE);
          c.r<4>()._v = c4._v;
          ftlb_ps = ps;
          require(c.r<4>().ie() > 1, "FTLB: missing TLBINV support\n");
          opts.ftlb() = true;
          opts.ftlbinv() = (c.r<4>().ie() == 3);
        }

      if (c.r<4>().ie() > 1)
        opts.tlbinv() = true;
    }

  if (is_boot_cpu)
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
      _ns_per_cycle = 1000000000 / frequency();
      _tlb_size = tlb_size + 1;
      options = opts;
      pr("TLB entries: %u\n", tlb_size + 1);
      if (opts.ftlb())
        {
          _ftlb_sets = 1U << (ftlb_info & 0xf);
          _ftlb_ways = (ftlb_info >> 4) + 2;
          pr("TLB: FTLB: page_size=%u sets=%u ways=%u\n",
             ftlb_ps, _ftlb_sets, _ftlb_ways);
        }

      _default_cca = c.r<0>().k0();

      // patch instruction alternatives for detected options
      Alternative_insn::handle_alternatives(opts._o);
    }
  else
    {
      require(_tlb_size == tlb_size + 1, "TLB size mismatch: %d <> %d\n",
              _tlb_size, tlb_size + 1);

      require(opts._o == options._o, "conflicting CPU options: %lx <> %lx\n",
              options._o, opts._o);
    }

  _phys_id = Proc::cpu_id();
  print_infos();
}

IMPLEMENT
void
Cpu::init(Cpu_number cpu, bool resume, bool is_boot_cpu)
{
  Unsigned32 prid = Mips::mfc0_32(Mips::Cp0_proc_id);
  for (Cpu_type *t = _types; t->hooks; ++t)
    {
      if ((prid & t->id_mask) == t->id)
        t->hooks->init(cpu, resume, prid);
    }

  if (!resume)
    first_boot(is_boot_cpu);

  if (options.segctl())
    {
      // setup segments for our purposes
      Mword cca = _default_cca;
      // kseg3 is currently kernel only mapped (used by JDB)
      // kseg kernel / user mapped
      Mips::mtc0(0x00300010, Mips::Cp0_seg_ctl_0);
      // kseg1 kernel unmapped uncached, user mapped
      // kseg0 kernel unmapped cached, user mapped
      // xam == 0 (UK)
      Mips::mtc0(0x00400042 | (cca << 16), Mips::Cp0_seg_ctl_1);

      // cached and mapped in user mode, xr == 0 (xphys disabled)
      Mips::mtc0(0x04300030 | (cca << 16) | cca, Mips::Cp0_seg_ctl_2);
      Mips::ehb();

      // now we should have a user address space up to 0xe0000000
    }

  Mips::mtc0(Mem_layout::Exception_base, Mips::Cp0_ebase);

  Cp0_status::write(Cp0_status::ST_DEFAULT);
  Mips::mtc0_32(0, Mips::Cp0_cause);
  Mips::ehb();

  Mword hwrena = 0xf;
  if (options.ulr())
    hwrena |= 0x20000000;

  /* Set the HW Enable register to allow rdhwr access from UM */
  Mips::mtc0_32(hwrena, Mips::Cp0_hw_rena);
}

