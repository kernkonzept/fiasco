#include "types.h"
#include "msr.h"

IMPLEMENTATION [ppc32]:

EXTENSION class Proc
{
public:
  //disable power savings mode
 static Mword wake(Mword);
 static Cpu_phys_id cpu_id();
};

/// Unblock external inetrrupts
IMPLEMENT static inline
void Proc::sti()
{
  Msr::set_msr_bit(Msr::Msr_ee);
}

/// Block external interrupts
IMPLEMENT static inline
void Proc::cli()
{
  Msr::clear_msr_bit(Msr::Msr_ee);
}

/// Are external interrupts enabled ?
IMPLEMENT static inline
Proc::Status Proc::interrupts()
{
  return (Status)Msr::read_msr() & Msr::Msr_ee;
}

/// Block external interrupts and save the old state
IMPLEMENT static inline
Proc::Status Proc::cli_save()
{
  Status ret = Msr::read_msr();
  Proc::cli();
  return ret;
}

/// Conditionally unblock external interrupts
IMPLEMENT static inline
void Proc::sti_restore(Status status)
{
  asm volatile ( "  mtmsr  %[status]                \n"
		 :
		 : [status] "r" (status)
		 );
}

IMPLEMENT static inline
void Proc::pause()
{
}

IMPLEMENT static inline
void Proc::halt()
{
  //enable interrupt and power saving mode in msr and wait for timer
  //exception
  Msr::set_msr_bit(Msr::Msr_ee | Msr::Msr_pow);
  while(Msr::read_msr() & Msr::Msr_pow)
    ;
}

IMPLEMENT static inline
Mword Proc::wake(Mword srr1)
{
  return srr1 & ~(Msr::Msr_ee | Msr::Msr_pow);
}

IMPLEMENT static inline
void Proc::irq_chance()
{
  asm volatile ("nop; nop;" : : :  "memory");
}

IMPLEMENT static inline
void Proc::stack_pointer(Mword sp)
{
  asm volatile ( " mr %%r1, %0 \n"
		 : : "r" (sp)
		 );
}

IMPLEMENT static inline
Mword Proc::stack_pointer()
{
  Mword sp;
  asm volatile ( " mr %0, %%r1  \n"
		 : "=r" (sp)
		 );
  return sp;
}

IMPLEMENT static inline
Mword Proc::program_counter()
{
  Mword pc;
  asm volatile ( " mflr %%r5 \n"
		 " bl   1f   \n"
		 "  1:       \n"
		 " mflr %0   \n"
		 " mtlr %%r5 \n"
		 : "=r" (pc) : : "r5");
  return pc;
}

IMPLEMENTATION [ppc32 && !mpcore]:

IMPLEMENT static inline
Cpu_phys_id Proc::cpu_id()
{ return Cpu_phys_id(0); }


