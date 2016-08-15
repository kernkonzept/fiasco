INTERFACE:

#include "types.h"

class Psr
{
public:
  enum Psr_bits
  {
    Impl           = 28, // implementation ID (read-only)
    Version        = 24, // version (read-only)
    Icc_neg        = 23, // Integer cond. code: negative
    Icc_zero       = 22, // Integer cond. code: zero
    Icc_overfl     = 21, // Integer cond. code: overflow
    Icc_carry      = 20, // Integer cond. code: carry
    Enable_copr    = 13, // Enable Coprocessor
    Enable_fpu     = 12, // Enable FPU
    Interrupt_lvl  =  8, // Interrupt level (above which we accept interrupts)
    Superuser      =  7, // Kernel mode
    Prev_superuser =  6, // Kernel mode state at most recent trap
    Enable_trap    =  5, // Traps on/off
  };

  enum Psr_masks
  {
    Impl_mask    = 0xF,
    Version_mask = 0xF,
    Irq_lvl_mask = 0xF,
    Cwp_mask     = 0x1F,
  };
};

//------------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC static inline
Mword
Psr::read()
{
  Mword psr = 0;
  asm volatile ("mov %%psr, %0" : "=r" (psr));
  return psr;
}

PUBLIC static inline
void
Psr::write(unsigned psr)
{
  asm volatile("mov %0, %%psr\n"
               "nop;nop;nop"
	       : : "r"(psr));
}

PUBLIC static inline
void
Psr::modify(Mword disable, Mword enable)
{
  write((read() & ~disable) | enable);
}
