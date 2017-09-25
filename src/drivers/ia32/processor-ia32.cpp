IMPLEMENTATION [ia32]:

#include "types.h"
#include "std_macros.h"

PUBLIC static inline
Cpu_phys_id
Proc::cpu_id()
{
  Mword eax, ebx,ecx, edx;
  asm volatile ("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                        : "a" (1));
  return Cpu_phys_id((ebx >> 24) & 0xff);
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
  asm volatile (".byte 0xf3, 0x90 #pause" );
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
