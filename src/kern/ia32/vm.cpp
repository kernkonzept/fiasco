INTERFACE:

#include "task.h"
#include "obj_space_phys_util.h"

class Vm : public Obj_space_phys_override<Task>
{
public:
  explicit Vm(Ram_quota *q)
  : Obj_space_phys_override<Task>(q, Caps::mem() | Caps::obj())
  {}

  int resume_vcpu(Context *, Vcpu_state *, bool) = 0;
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "cpu.h"

PUBLIC inline
Page_number
Vm::mem_space_map_max_address() const
{ return Page_number(1UL << (MWORD_BITS - Mem_space::Page_shift)); }

PROTECTED static inline
void
Vm::force_kern_entry_vcpu_state(Vcpu_state *vcpu)
{
  vcpu->state &= ~(Vcpu_state::F_traps | Vcpu_state::F_user_mode);
}

/*
 * Sanitize and load a guest-provided xcr0 value.
 *
 * An invalid xcr0 value will cause a GP. Therefore we implement all the checks
 * defined in the intel manual to ensure that no guest can cause a GP.
 *
 * This function is strict, and does not allow for "new" bits that come up in
 * new CPUs and are advertised in cpuid. These new bits could come with new
 * constraints that need to be checked here. Therefore even if those bits are
 * enabled in the host, we do not allow them for the guest until we get the new
 * manual and can implement the integrity checks accordingly.
 */
PROTECTED static inline NEEDS["cpu.h"]
void
Vm::load_guest_xcr0(Unsigned64 host_xcr0, Unsigned64 guest_xcr0)
{
  if (!Cpu::have_xsave() || !(host_xcr0 ^ guest_xcr0))
    return;

  enum : Unsigned64
  {
    Xstate_fp           = 1 << 0,
    Xstate_sse          = 1 << 1,
    Xstate_avx          = 1 << 2,
    Xstate_defined_bits = 0x7,
  };
  guest_xcr0 &= Xstate_defined_bits; // allow only defined bits
  guest_xcr0 |= Xstate_fp;           // fp must always be set
  if (guest_xcr0 & Xstate_avx)       // if avx is set, sse must also be set
    guest_xcr0 |= Xstate_sse;

  guest_xcr0 &= host_xcr0; // only allow bits that are available on the CPU
  Cpu::xsetbv(guest_xcr0, 0);
}

/*
 * Load host xcr0.
 */
PROTECTED static inline NEEDS["cpu.h"]
void
Vm::load_host_xcr0(Unsigned64 host_xcr0, Unsigned64 guest_xcr0)
{
  if (!Cpu::have_xsave() || !(guest_xcr0 ^ host_xcr0))
    return;
  Cpu::xsetbv(host_xcr0, 0);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [ia32]:

PROTECTED static inline
bool
Vm::is_64bit()
{ return false; }

// ------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

PROTECTED static inline
bool
Vm::is_64bit()
{ return true; }
