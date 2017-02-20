INTERFACE [mips && perf_cnt]:

#include "cpu.h"
#include "initcalls.h"

#include <cxx/bitfield>

EXTENSION class Perf_cnt
{
private:
  struct Ctl_reg
  {
    Unsigned32 _v;
    Ctl_reg() = default;
    explicit Ctl_reg(Unsigned32 v) : _v(v) {}

    CXX_BITFIELD_MEMBER( 0,  0, exl, _v);
    CXX_BITFIELD_MEMBER( 1,  1, k, _v);
    CXX_BITFIELD_MEMBER( 2,  2, s, _v);
    CXX_BITFIELD_MEMBER( 3,  3, u, _v);
    CXX_BITFIELD_MEMBER( 4,  4, ie, _v);
    CXX_BITFIELD_MEMBER( 5, 12, event, _v);
    CXX_BITFIELD_MEMBER(15, 15, pctd, _v);
    CXX_BITFIELD_MEMBER(23, 24, ec, _v);
    CXX_BITFIELD_MEMBER(30, 30, w, _v);
    CXX_BITFIELD_MEMBER(31, 31, m, _v);
  };

  static bool _ec_avail;
  static bool _wide_counter;

  static Perf_read_fn read_pmc[Max_slot];
  static Per_cpu<bool[2]> _using_odd;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [mips && perf_cnt]:

#include "tb_entry.h"

#include <cstdio>
#include <string.h>

DEFINE_PER_CPU Per_cpu<bool[2]> Perf_cnt::_using_odd;

Perf_cnt::Perf_read_fn Perf_cnt::read_pmc[Max_slot] =
{ dummy_read_pmc, dummy_read_pmc, };

bool Perf_cnt::_ec_avail;
bool Perf_cnt::_wide_counter;

static Mword dummy_read_pmc() { return 0; }

PUBLIC static FIASCO_INIT_CPU
void
Perf_cnt::init_ap()
{
  if (current_cpu() != Cpu_number::boot_cpu())
    return;

  if (!Mips::Cfg<1>::read().pc())
    return;

  unsigned num_perf_regs = 1;

  Ctl_reg r(Mips::mfc0_32(Mips::Cp0_perf_ctl_0));
  write_counter(0, 0);
  if (r.m())
    {
      ++num_perf_regs;
      write_counter(0, 1);
      r = Ctl_reg(Mips::mfc0_32(Mips::Cp0_perf_ctl_1));
      if (r.m())
        {
          ++num_perf_regs;
          write_counter(0, 2);
          r = Ctl_reg(Mips::mfc0_32(Mips::Cp0_perf_ctl_2));
          if (r.m())
            {
              ++num_perf_regs;
              write_counter(0, 3);
            }
        }
    }

  printf("Detected %d performance monitor registers.\n", num_perf_regs);

  r = Ctl_reg(0);
  r.ec() = 1;
  Mips::mtc0_32(r._v, Mips::Cp0_perf_ctl_0);
  r = Ctl_reg(Mips::mfc0_32(Mips::Cp0_perf_ctl_0));
  _ec_avail     = r.ec() != 0;
  // Only 64bit systems can have 32 or 64bit wide counters
  _wide_counter = r.w();

  read_pmc[0] = read_counter_0;
  read_pmc[1] = read_counter_1;
}

PUBLIC static void
Perf_cnt::get_unit_mask(Mword, Unit_mask_type *type, Mword *, Mword *)
{
  *type = Perf_cnt::Fixed;
}

PUBLIC static void
Perf_cnt::get_unit_mask_entry(Mword, Mword, Mword *value, const char **desc)
{
  *value = 0;
  *desc  = 0;
}

PUBLIC static void
Perf_cnt::get_perf_event(Mword nr, unsigned *evntsel,
                         const char **name, const char **desc)
{
  // having one set of static strings in here is ok
  static char _name[20];
  static char _desc[50];

  switch (nr)
    {
    case 0:
    case 1:
      strncpy(_name, "Cycles", sizeof(_name));
      strncpy(_desc, "Cycles", sizeof(_desc));
      break;
    case 2:
    case 3:
      strncpy(_name, "Instructions", sizeof(_name));
      strncpy(_desc,  "Instructions graduated", sizeof(_desc));
      break;
    default:
      snprintf(_name, sizeof(_name),
               "Event-%ld-%s", nr >> 1, (nr & 1) ? "Odd" : "Even");
      snprintf(_desc, sizeof(_desc),
               "%s variant of event %ld", (nr & 1) ? "Odd" : "Even", nr >> 1);
    }

  _name[sizeof(_name) - 1] = 0;
  _desc[sizeof(_desc) - 1] = 0;

  *name = (const char *)&_name;
  *desc = (const char *)&_desc;
  *evntsel = nr;
}

PUBLIC static void
Perf_cnt::split_event(Mword event, unsigned *evntsel, Mword *)
{
  *evntsel = event;
}

PUBLIC static void
Perf_cnt::combine_event(Mword evntsel, Mword, Mword *event)
{
  *event = evntsel;
}

PUBLIC static Mword
Perf_cnt::lookup_event(Mword) { return 0; }

PUBLIC static
Mword
Perf_cnt::read_counter_0()
{ return read_counter(0); }

PRIVATE static
Mword
Perf_cnt::read_counter_1()
{ return read_counter(1); }

PUBLIC static
void
Perf_cnt::write_counter32(Unsigned32 v, int counter_nr)
{
  switch (counter_nr)
    {
    default:
    case 0: Mips::mtc0_32(v, Mips::Cp0_perf_counter_0); break;
    case 1: Mips::mtc0_32(v, Mips::Cp0_perf_counter_1); break;
    case 2: Mips::mtc0_32(v, Mips::Cp0_perf_counter_2); break;
    case 3: Mips::mtc0_32(v, Mips::Cp0_perf_counter_3); break;
    };
}

PUBLIC static
void
Perf_cnt::write_counter64(Mword v, int counter_nr)
{
  switch (counter_nr)
    {
    default:
    case 0: Mips::mtc0(v, Mips::Cp0_perf_counter_0); break;
    case 1: Mips::mtc0(v, Mips::Cp0_perf_counter_1); break;
    case 2: Mips::mtc0(v, Mips::Cp0_perf_counter_2); break;
    case 3: Mips::mtc0(v, Mips::Cp0_perf_counter_3); break;
    };
}

PUBLIC static
void
Perf_cnt::write_counter(Mword v, int counter_nr)
{
  if (_wide_counter)
    write_counter64(v, counter_nr);
  else
    write_counter32(v, counter_nr);
}

PRIVATE static
unsigned long
Perf_cnt::read_counter32(int counter_nr)
{
  switch (counter_nr)
    {
    default:
    case 0: return Mips::mfc0_32(Mips::Cp0_perf_counter_0);
    case 1: return Mips::mfc0_32(Mips::Cp0_perf_counter_1);
    case 2: return Mips::mfc0_32(Mips::Cp0_perf_counter_2);
    case 3: return Mips::mfc0_32(Mips::Cp0_perf_counter_3);
    };
}

PRIVATE static
unsigned long
Perf_cnt::read_counter64(int counter_nr)
{
  switch (counter_nr)
    {
    default:
    case 0: return Mips::mfc0(Mips::Cp0_perf_counter_0);
    case 1: return Mips::mfc0(Mips::Cp0_perf_counter_1);
    case 2: return Mips::mfc0(Mips::Cp0_perf_counter_2);
    case 3: return Mips::mfc0(Mips::Cp0_perf_counter_3);
    };
}

PUBLIC static
unsigned long
Perf_cnt::read_counter(int counter_nr)
{
  if (_wide_counter)
    return read_counter64(counter_nr);
  return read_counter32(counter_nr);
}

PRIVATE static inline
void
Perf_cnt::write_ctl(Ctl_reg const &v, unsigned num)
{
  switch (num)
    {
    case 0: Mips::mtc0_32(v._v, Mips::Cp0_perf_ctl_0); break;
    case 1: Mips::mtc0_32(v._v, Mips::Cp0_perf_ctl_1); break;
    case 2: Mips::mtc0_32(v._v, Mips::Cp0_perf_ctl_2); break;
    case 3: Mips::mtc0_32(v._v, Mips::Cp0_perf_ctl_3); break;
    };
}

PRIVATE static inline
Perf_cnt::Ctl_reg
Perf_cnt::read_ctl(unsigned num)
{
  switch (num)
    {
    default:
    case 0: return Ctl_reg(Mips::mfc0_32(Mips::Cp0_perf_ctl_0));
    case 1: return Ctl_reg(Mips::mfc0_32(Mips::Cp0_perf_ctl_1));
    case 2: return Ctl_reg(Mips::mfc0_32(Mips::Cp0_perf_ctl_2));
    case 3: return Ctl_reg(Mips::mfc0_32(Mips::Cp0_perf_ctl_3));
    };
}

PUBLIC static
int
Perf_cnt::setup_pmc(Mword slot, Mword event, Mword user, Mword kern, Mword)
{
  if (slot >= Max_slot)
    return 0;

  if (slot >= 2)
    return 0; // P5600 requirement for 2 perf pairs

  Ctl_reg v(0);
  v.exl()   = kern;
  v.k()     = kern;
  v.s()     = kern;
  v.u()     = user;
  v.event() = event >> 1;
  if (_ec_avail)
    v.ec()  = 3;

  _using_odd.current()[slot] = event & 1;

  write_ctl(v, slot * 2 + _using_odd.current()[slot]);

  Tb_entry::set_rdcnt(slot, read_pmc[slot]);

  return 1;
}

PUBLIC static int
Perf_cnt::mode(Mword slot, const char **mode, const char **name,
               Mword *event, Mword *user, Mword *kern, Mword *edge)
{
  if (slot >= Max_slot)
    return 0;

  Ctl_reg v = read_ctl(slot * 2);

  // could consider EC too
  if (v.k() && v.u())
    *mode = "all";
  else if (v.k())
    *mode = "kern";
  else if (v.u())
    *mode = "user";
  else
    *mode = "off";

  static char namebuf[Max_slot][20];
  switch (v.event())
    {
    case 0:
    case 1: *name = "Cycles";
            break;
    case 2:
    case 3: *name = "Insns";
            break;
    default:
            snprintf(namebuf[slot], sizeof(namebuf[slot]),
                     "Event-%d-%s", (int)v.event(),
                     _using_odd.current()[slot] ? "Odd" : "Even");
            namebuf[slot][sizeof(namebuf[slot]) - 1] = 0;
            *name = namebuf[slot];
            break;
    }

  *event = v.event() * 2 + _using_odd.current()[slot];
  *user = v.u();
  *kern = v.k();
  *edge = 0;

  return 1;
}

PUBLIC static char const *
Perf_cnt::perf_type() { return "MIPS"; }

// ------------------------------------------------------------------------
IMPLEMENTATION [mips && perf_cnt && !cpu_virt]:

PUBLIC static inline
Mword Perf_cnt::get_max_perf_event() { return 2 * 118; } // P5600

// ------------------------------------------------------------------------
IMPLEMENTATION [mips && perf_cnt && cpu_virt]:

PUBLIC static inline
Mword Perf_cnt::get_max_perf_event() { return 2 * 139; } // P5600
