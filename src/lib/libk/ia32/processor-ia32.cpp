IMPLEMENTATION [ia32]:

#include "types.h"
#include "std_macros.h"

/**
 * Return CPU ID.
 *
 * Depending on the platform, this is either a 6-bit, an 8-bit, or a 32-bit
 * value. Note that the 32-bit x2APIC CPU ID requires 2 invocations of CPUID.
 */
PUBLIC static inline
Cpu_phys_id
Proc::cpu_id()
{
  Unsigned32 eax, ebx, ecx, edx;
  cpuid(1, &eax, &ebx, &ecx, &edx);
  if (eax >= 0xb)
    return Cpu_phys_id(cpuid_edx(0xb));
  else
    return Cpu_phys_id(ebx >> 24);
}

IMPLEMENT static inline
Mword Proc::stack_pointer()
{
  Mword sp;
  asm volatile ("movl %%esp, %0" : "=r" (sp));
  return sp;
}

IMPLEMENT static inline
void Proc::stack_pointer(Mword sp)
{
  asm volatile ("movl %0, %%esp" : : "r" (sp));
}

IMPLEMENT static inline
Mword Proc::program_counter()
{
  Mword pc;
  asm volatile ("call 1f; 1: pop %0" : "=r" (pc));
  return pc;
}

IMPLEMENT static inline
void Proc::pause()
{
  asm volatile ("pause");
}

IMPLEMENT static inline
void Proc::halt()
{
  asm volatile ("hlt");
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
  Status ret;
  asm volatile ("pushfl   \n\t"
                "popl %0  \n\t"
                "cli      \n\t"
                : "=g" (ret) : /* no input */ : "memory");
  return ret;
}

IMPLEMENT static inline
void Proc::sti_restore(Status st)
{
  if (st & 0x0200)
    asm volatile ("sti" : : : "memory");
}

IMPLEMENT static inline
Proc::Status Proc::interrupts()
{
  Status ret;
  asm volatile ("pushfl    \n\t"
                "popl %0   \n\t"
                : "=g" (ret) : /* no input */ : "memory");
  return ret & 0x0200;
}

IMPLEMENT static inline
void Proc::irq_chance()
{
  asm volatile ("nop");
  pause();
}

PUBLIC static inline
Unsigned64
Proc::rdmsr(Unsigned32 msr)
{
  Unsigned64 v;
  asm volatile ("rdmsr" : "=A" (v) : "c" (msr));
  return v;
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
