/*
 * Fiasco-UX
 * Architecture specific cpu init code
 */

INTERFACE[ux]:

class Tss;

EXTENSION class Cpu
{
public:
  static Mword kern_ds() { return _kern_ds; }
  static Mword kern_es() { return _kern_es; }
private:
  static Tss *tss asm ("CPU_TSS");
  static int msr_dev;
  static unsigned long _gs asm ("CPU_GS");
  static unsigned long _fs asm ("CPU_FS");
  static Mword _kern_ds asm ("KERN_DS");
  static Mword _kern_es asm ("KERN_ES");

  Cpu_phys_id _tid;
};

// -----------------------------------------------------------------------
IMPLEMENTATION[ux]:

#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>

#include "div32.h"
#include "emulation.h"
#include "gdt.h"
#include "initcalls.h"
#include "processor.h"
#include "regdefs.h"
#include "tss.h"

Proc::Status volatile Proc::virtual_processor_state = 0;
Tss *Cpu::tss;
int Cpu::msr_dev = -1;
unsigned long Cpu::_gs; // for mp: percpu
unsigned long Cpu::_fs; // for mp: percpu
unsigned long Cpu::_kern_ds;
unsigned long Cpu::_kern_es;

PRIVATE inline ALWAYS_INLINE
void
Cpu::try_enable_hw_performance_states(bool) {}

IMPLEMENT FIASCO_INIT_CPU
void
Cpu::init()
{
  identify();

  // Get original es and ds to be used for RESET_KERNEL_SEGMENTS_FORCE_DS_ES
  _kern_ds = get_ds();
  _kern_es = get_es();

  // No Sysenter Support for Fiasco-UX
  _features &= ~FEAT_SEP;

  // Determine CPU frequency
  FILE *fp;
  if ((fp = fopen ("/proc/cpuinfo", "r")) != NULL)
    {
      char buffer[128];
      float val;

      while (fgets (buffer, sizeof (buffer), fp))
	if (sscanf (buffer, "cpu MHz%*[^:]: %f", &val) == 1)
	  {
	    _frequency    = (Unsigned64)(val * 1000) * 1000;
	    scaler_tsc_to_ns = muldiv (1 << 27, 1000000000,    _frequency);
	    scaler_tsc_to_us = muldiv (1 << 27, 32 * 1000000,  _frequency);
	    scaler_ns_to_tsc = muldiv (1 << 27, _frequency, 1000000000);
	    break;
	  }

      fclose (fp);
    }

  if (this == _boot_cpu)
    {
      // XXX hardcoded
      msr_dev = open("/dev/msr", O_RDWR);
      if (msr_dev == -1)
	msr_dev = open("/dev/cpu/0/msr", O_RDWR);
    }

  _tid = phys_id_direct();
}

PUBLIC
void
Cpu::print_infos() const
{
  printf("CPU[%u:%u]: %s (%X:%X:%X:%X) Model: %s at %llu MHz\n\n",
         cxx::int_value<Cpu_number>(id()),
         cxx::int_value<Cpu_phys_id>(phys_id()),
         vendor_str(), family(), model(), stepping(), brand(), model_str(),
         div32(frequency(), 1000000));
}

PUBLIC inline
Cpu_phys_id
Cpu::phys_id() const
{ return _tid; }

PUBLIC static inline
void
Cpu::set_fast_entry (void (*)(void))
{}

PUBLIC static FIASCO_INIT
void
Cpu::init_tss (Address tss_mem)
{
  tss = reinterpret_cast<Tss*>(tss_mem);
  tss->_ss0 = Gdt::gdt_data_kernel;
}

PUBLIC static inline
Tss*
Cpu::get_tss ()
{ return tss; }

PUBLIC static inline
void
Cpu::enable_rdpmc()
{
}

IMPLEMENT inline
int
Cpu::can_wrmsr() const
{
  return msr_dev != -1;
}

PUBLIC static
Unsigned64
Cpu::rdmsr (Unsigned32 reg)
{
  Unsigned64 msr;

  if (lseek(msr_dev, reg, SEEK_SET) >= 0)
    read(msr_dev, &msr, sizeof(msr));

  return msr;
}

PUBLIC static inline
Unsigned64
Cpu::rdpmc (Unsigned32, Unsigned32 reg)
{
  return rdmsr(reg);
}

PUBLIC static
void
Cpu::wrmsr (Unsigned64 msr, Unsigned32 reg)
{
  if (lseek(msr_dev, reg, SEEK_SET) >= 0)
    write(msr_dev, &msr, sizeof(msr));
}

PUBLIC static inline
void
Cpu::wrmsr (Unsigned32 low, Unsigned32 high, Unsigned32 reg)
{
  Unsigned64 msr = ((Unsigned64)high << 32) | low;
  wrmsr(msr, reg);
}

PUBLIC static inline
void
Cpu::debugctl_enable()
{}

PUBLIC static inline
void
Cpu::debugctl_disable()
{}

PUBLIC static inline
void
Cpu::set_fs(Unsigned32 val)
{ _fs = val; }

PUBLIC static inline
void
Cpu::set_gs(Unsigned32 val)
{ _gs = val; }

PUBLIC static inline
Unsigned32
Cpu::get_fs()
{ return _fs; }

PUBLIC static inline
Unsigned32
Cpu::get_gs()
{ return _gs; }

// ------------------------------------------------------------------------
IMPLEMENTATION[ux && !mp]:

PUBLIC static inline
Cpu_phys_id
Cpu::phys_id_direct()
{ return Cpu_phys_id(0); }

// ------------------------------------------------------------------------
IMPLEMENTATION[ux && mp]:

PUBLIC static inline NEEDS["emulation.h"]
Cpu_phys_id
Cpu::phys_id_direct()
{ return Cpu_phys_id(Emulation::gettid()); }

