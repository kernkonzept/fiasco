/*
 * Fiasco-UX
 * Architecture specific floating point unit code
 */

IMPLEMENTATION[ux && fpu]:

#include <cassert>
#include <sys/ptrace.h>
#include "cpu.h"
#include "regdefs.h"
#include "space.h"

PRIVATE static
void
Fpu::init_xsave(Cpu_number)
{}

PRIVATE static
void
Fpu::init_disable()
{}

/**
 * Save FPU state in Fpu_state s. Distinguish between i387 and SSE state
 * @param s Fpu_state to save context in
 */
IMPLEMENT
void
Fpu::save_state(Fpu_state *s)
{
  assert (s->state_buffer());

  // FIXME: assume UP here (current_meme_space(0))
  ptrace(Cpu::boot_cpu()->features() & FEAT_FXSR ? PTRACE_GETFPXREGS : PTRACE_GETFPREGS,
         Mem_space::current_mem_space(Cpu_number::boot_cpu())->pid(), NULL, s->state_buffer());
}

/**
 * Restore FPU state from Fpu_state s. Distinguish between i387 and SSE state
 * @param s Fpu_state to restore context from
 */
IMPLEMENT
void
Fpu::restore_state(Fpu_state *s)
{
  assert (s->state_buffer());

  // FIXME: assume UP here (current_meme_space(0))
  ptrace(Cpu::boot_cpu()->features() & FEAT_FXSR ? PTRACE_SETFPXREGS : PTRACE_SETFPREGS,
         Mem_space::current_mem_space(Cpu_number::boot_cpu())->pid(), NULL, s->state_buffer());
}

/**
 * Disable FPU. Does nothing here.
 */
PUBLIC static inline
void
Fpu::disable()
{}

/**
 * Enable FPU. Does nothing here.
 */
PUBLIC static inline
void
Fpu::enable()
{}
