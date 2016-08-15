INTERFACE [sparc]:

#include "per_cpu_data.h"
#include "types.h"

EXTENSION class Cpu
{
public:
  void init(bool resume, bool is_boot_cpu = false);
  static void early_init();

  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  Cpu(Cpu_number cpu) { set_id(cpu); }

private:
  static Cpu *_boot_cpu;
  Cpu_phys_id _phys_id;
  static unsigned long _ns_per_cycle;

};

namespace Segment
{
  enum Attribs_enum
  {
    Ks = 1UL << 30,
    Kp = 1UL << 29,
    N  = 1UL << 28,
    Default_attribs = Kp //Ks | Kp,
  };
};
//------------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include "panic.h"
#include "psr.h"

#include <cstdio>

DEFINE_PER_CPU Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
Cpu *Cpu::_boot_cpu;
unsigned long Cpu::_ns_per_cycle;

PUBLIC static inline unsigned Cpu::phys_bits() { return 32; }

IMPLEMENT
void
Cpu::init(bool, bool is_boot_cpu)
{
  if (is_boot_cpu)
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
    }

  _phys_id = Cpu_phys_id(0); //Proc::cpu_id();

  _ns_per_cycle = 0; //1000000000 / Boot_info::get_time_base();
  //printf("Timebase: %lu\n", Boot_info::get_time_base());

  Psr::modify(0, 1ul << Psr::Enable_trap);
}

PUBLIC inline
Cpu_phys_id
Cpu::phys_id() const
{ return _phys_id; }

IMPLEMENT
void
Cpu::early_init()
{
}

PUBLIC static inline
Mword
Cpu::read_vsid(unsigned sr = 0)
{
  Mword vsid;
  (void)sr;

  return (vsid & 0xffffff);
}

/* set segment register 0-15 */
PUBLIC static inline //NEEDS["paging.h"]
void
Cpu::set_vsid(Mword vsid)
{
  vsid |= Segment::Default_attribs;
}

PUBLIC static inline
Mword
Cpu::stack_align(Mword stack)
{ return stack & ~0xf; }

PUBLIC static inline
bool
Cpu::have_superpages()
{ return true; }

//------------------------------------------------------------------------------
/* Time functions */

/**
 * Read time base registers 
 */
PUBLIC static inline
Unsigned64
Cpu::rdtsc()
{
  Unsigned32 tb_upper, tb_lower;
  Unsigned64 tb;
  tb = tb_upper;
  return (tb << 32) | tb_lower;
}

PUBLIC static inline
void
Cpu::busy_wait_ns(Unsigned64 ns)
{
  Unsigned64 stop = rdtsc() + ns_to_tsc(ns);

  while(rdtsc() <  stop) 
    ;
}

PUBLIC static inline
Unsigned64
Cpu::ns_to_tsc(Unsigned64 ns)
{
  return ns / _ns_per_cycle;
}

PUBLIC static inline
Unsigned64
Cpu::tsc_to_ns(Unsigned64 tsc)
{
  return tsc * _ns_per_cycle;
}

PUBLIC static inline
Unsigned32
Cpu::get_scaler_tsc_to_ns()
{ return 0; }

PUBLIC static inline
Unsigned32
Cpu::get_scaler_tsc_to_us() 
{ return 0; }

PUBLIC static inline
Unsigned32 
Cpu::get_scaler_ns_to_tsc() 
{ return 0; }

PUBLIC static inline
bool
Cpu::tsc()
{ return 0; }

//------------------------------------------------------------------------------
/* Unimplemented */

PUBLIC static inline
void
Cpu::debugctl_enable()
{}

PUBLIC static inline
void
Cpu::debugctl_disable()
{}

