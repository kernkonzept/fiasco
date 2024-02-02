INTERFACE [ux]:

#include "types.h"

EXTENSION class Proc
{
  static Status volatile virtual_processor_state;
};

IMPLEMENTATION [ux]:

#include <time.h>

#include "types.h"
#include "std_macros.h"


IMPLEMENT static inline
void Proc::stack_pointer(Mword sp)
{
  asm volatile ( "movl %0, %%esp \n" : : "r"(sp) );
}

IMPLEMENT static inline
Mword Proc::stack_pointer()
{
  Mword res;
  asm volatile ( "movl %%esp, %0 \n" : "=r" (res) );
  return res;
}

IMPLEMENT static inline
Mword Proc::program_counter ()
{
  Mword pc;
  asm volatile ("call 1f ; 1: pop %0" : "=rm"(pc));
  return pc;
}

PUBLIC static inline
Proc::Status Proc::processor_state()
{
  return virtual_processor_state;
}

PUBLIC static inline
void Proc::ux_set_virtual_processor_state(Status s)
{
  virtual_processor_state = s;
}

IMPLEMENT static inline
void Proc::pause()
{
  asm volatile ("pause");
}

IMPLEMENT static inline
void Proc::halt()
{
  static struct timespec idle;
  idle.tv_sec  = 10;
  idle.tv_nsec = 0;
  nanosleep (&idle, NULL);
}

IMPLEMENT static inline
void Proc::cli()
{
  asm volatile ("cli" : : : "memory");
}

IMPLEMENT static inline
void Proc::sti()
{
  asm volatile ("sti" : : : "memory");
}

IMPLEMENT static inline
Proc::Status Proc::cli_save()
{
  Status ret = virtual_processor_state;
  cli();
  return ret;
}

IMPLEMENT static inline
void Proc::sti_restore(Status st)
{
  if (st & 0x200)
    Proc::sti();
}

IMPLEMENT static inline
Proc::Status Proc::interrupts()
{
  return virtual_processor_state & 0x0200;
}

IMPLEMENT static inline NEEDS[<time.h>]
void Proc::irq_chance()
{
  asm volatile ("nop; nop;" : : : "memory");
}

PUBLIC static inline
Unsigned64
Proc::rdmsr(Unsigned32 msr)
{
  Unsigned32 h, l;
  asm volatile ("rdmsr" : "=a" (l), "=d" (h) : "c" (msr));
  return (Unsigned64{h} << 32) | l;
}

PUBLIC static inline
void
Proc::wrmsr(Unsigned64 value, Unsigned32 msr)
{
  asm volatile ("wrmsr": :
                "a" (static_cast<Unsigned32>(value)),
                "d" (static_cast<Unsigned32>(value >> 32)),
                "c" (msr));
}

PUBLIC static inline
void
Proc::cpuid(Unsigned32 mode, Unsigned32 ecx_val,
            Unsigned32 *eax, Unsigned32 *ebx, Unsigned32 *ecx, Unsigned32 *edx)
{
  asm volatile ("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                        : "a" (mode), "c" (ecx_val));
}

PUBLIC static inline
void
Proc::cpuid(Unsigned32 mode,
            Unsigned32 *eax, Unsigned32 *ebx, Unsigned32 *ecx, Unsigned32 *edx)
{ cpuid(mode, 0, eax, ebx, ecx, edx); }

PUBLIC static inline
Unsigned32
Proc::cpuid_eax(Unsigned32 mode)
{
  Unsigned32 eax, dummy;
  cpuid(mode, &eax, &dummy, &dummy, &dummy);
  return eax;
}

PUBLIC static inline
Unsigned32
Proc::cpuid_ebx(Unsigned32 mode)
{
  Unsigned32 ebx, dummy;
  cpuid(mode, &dummy, &ebx, &dummy, &dummy);
  return ebx;
}

PUBLIC static inline
Unsigned32
Proc::cpuid_ecx(Unsigned32 mode)
{
  Unsigned32 ecx, dummy;
  cpuid(mode, &dummy, &dummy, &ecx, &dummy);
  return ecx;
}

PUBLIC static inline
Unsigned32
Proc::cpuid_edx(Unsigned32 mode)
{
  Unsigned32 edx, dummy;
  cpuid(mode, &dummy, &dummy, &dummy, &edx);
  return edx;
}
