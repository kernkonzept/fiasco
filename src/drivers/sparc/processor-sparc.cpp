#include "types.h"
#include "psr.h"
#include "processor.h"

IMPLEMENTATION [sparc]:

EXTENSION class Proc
{
public:
  //disable power savings mode
 static Mword wake(Mword);
 static Cpu_phys_id cpu_id();
};

/// Unblock external interrupts
IMPLEMENT static inline
void Proc::sti()
{
  unsigned p = Psr::read();
  p &= ~(0xF << Psr::Interrupt_lvl);
  Psr::write(p);
}

/// Block external interrupts
IMPLEMENT static inline
void Proc::cli()
{
  unsigned p = Psr::read();
  p |= (0xF << Psr::Interrupt_lvl);
  Psr::write(p);
}

/// Are external interrupts enabled ?
IMPLEMENT static inline
Proc::Status Proc::interrupts()
{
  return Psr::read() & (0xF << Psr::Interrupt_lvl);
}

/// Block external interrupts and save the old state
IMPLEMENT static inline
Proc::Status Proc::cli_save()
{
  Status ret = Psr::read();
  Proc::cli();
  return ret;
}

/// Conditionally unblock external interrupts
IMPLEMENT static inline
void Proc::sti_restore(Status status)
{
  (void)status;
  Psr::write(status);
}

IMPLEMENT static inline
void Proc::pause()
{
  // XXX
}

IMPLEMENT static inline
void Proc::halt()
{
  // XXX
  asm volatile ("ta 0\n");
}

IMPLEMENT static inline
Mword Proc::wake(Mword srr1)
{
  (void)srr1;
  return 0; // XXX
}

IMPLEMENT static inline
void Proc::irq_chance()
{
  // XXX?
  asm volatile ("nop; nop;" : : :  "memory");
}

IMPLEMENT static inline
void Proc::stack_pointer(Mword sp)
{
  (void)sp;
  asm volatile ("mov %0, %%sp\n" : : "r"(sp));
}

IMPLEMENT static inline
Mword Proc::stack_pointer()
{
  Mword sp = 0;
  asm volatile ("mov %%sp, %0\n" : "=r" (sp));
  return sp;
}

IMPLEMENT static inline
Mword Proc::program_counter()
{
  Mword pc = 0;
  asm volatile ("call 1\n\t"
                "nop\n\t" // delay instruction
		"1: mov %%o7, %0\n\t"
		: "=r" (pc) : : "o7");
  return pc;
}

PUBLIC static inline
template <int ASI>
Mword Proc::read_alternative(Mword reg)
{
	Mword ret;
	asm volatile("lda [%1] %2, %0"
				  : "=r" (ret)
				  : "r" (reg),
				    "i" (ASI));
	return ret;

}

PUBLIC static inline
template <int ASI>
void Proc::write_alternative(Mword reg, Mword value)
{
	asm volatile ("sta %0, [%1] %2\n\t"
				  :
				  : "r"(value),
				    "r"(reg),
				    "i"(ASI));
}


IMPLEMENTATION [sparc && !mp]:

IMPLEMENT static inline
Cpu_phys_id Proc::cpu_id()
{ return Cpu_phys_id(0); }
