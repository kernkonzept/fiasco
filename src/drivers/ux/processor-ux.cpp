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
  asm volatile (" .byte 0xf3, 0x90 #pause \n" );
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
