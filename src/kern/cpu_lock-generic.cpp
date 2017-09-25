/**
 * Generic implementation of the CPU lock.
 * This implementation uses Proc::cli and
 * Proc::sti from the processor headers.
 */
IMPLEMENTATION:

#include "processor.h"

IMPLEMENT inline
Cpu_lock::Cpu_lock()
{}

IMPLEMENT inline NEEDS ["processor.h"]
void Cpu_lock::lock()
{
  Proc::cli();
}

IMPLEMENT inline NEEDS ["processor.h"]
void
Cpu_lock::clear()
{
  Proc::sti();
}

IMPLEMENT inline NEEDS ["processor.h"]
Cpu_lock::Status Cpu_lock::test() const
{
  return !Proc::interrupts();
}
