INTERFACE[mips]:

EXTENSION class Context
{
protected:
  Mword _ulr;
  // must not collide with any bit in Thread_state
  enum { Thread_vcpu_vz_owner = 0x4000000 };
};

IMPLEMENTATION [mips]:

#include "asm_mips.h"
#include "processor.h"

PUBLIC inline
void
Context::prepare_switch_to(void (*fptr)())
{
#ifndef __mips64
  // keep the stack pointer 64-bit aligned
  --_kernel_sp;
#endif
  *reinterpret_cast<void(**)()> (--_kernel_sp) = fptr;
}

IMPLEMENT inline
void
Context::spill_user_state()
{}

IMPLEMENT inline
void
Context::fill_user_state()
{}

PROTECTED inline void Context::arch_setup_utcb_ptr()
{
  // simulate the TLS model + TCB layout:
  // the thread pointer points + 0x7000 after the end of the TCB
  // (2x void*: dtv ptr and private ptr) before that we shall store
  // the UTCB pointer in user-land TLS and we simulate this...
  _ulr = reinterpret_cast<Address>(&_utcb.usr()->utcb_addr)
         + 0x7000 + (3 * sizeof(void*));
  _utcb.kern()->utcb_addr = reinterpret_cast<Mword>(_utcb.usr().get());
}

IMPLEMENT inline NEEDS["processor.h", "asm_mips.h"]
void
Context::switch_cpu(Context *t)
{
  Proc::set_ulr(t->_ulr);

    {
      Mword register _old_this asm("$5") =
        reinterpret_cast<Mword>(this);          // a1
      Mword register _new_this asm("$4") =
        reinterpret_cast<Mword>(t);             // a0
      Mword register _old_sp asm("$6") =
        reinterpret_cast<Mword>(&_kernel_sp);   // a2
      Mword register _new_sp asm("$7") =
        reinterpret_cast<Mword>(t->_kernel_sp); // a3

      __asm__ __volatile__ (
          ".set push                                  \n"
          ".set noreorder                             \n"
          ".set noat                                  \n"
          // Stack (MIPS32: 7 words, MIPS64: 6 words):
          //   - 4 words: Parameter space for Context::switch_context(). This
          //     stack space is reserved by the caller to allow the callee to
          //     store $a0..$a3.
          //   - Only MIPS32: 1 word to keep the stack for the callee 64-bit
          //                  aligned. See also Context::prepare_switch_to().
          //   - 1 word: $28 / $gp.
          //   - 1 word: $29 / $sp.
          "  " ASM_ADDIU " $29, $29, -(%[rsz] * (%[algn]+6)) \n"
          "  " ASM_S     " $29, (%[old_sp])           \n"
          "  " ASM_S     " $28, (%[rsz] * (%[algn]+4))($29) \n"
          "  " ASM_S     " $30, (%[rsz] * (%[algn]+5))($29) \n"
          "  " ASM_LA    " $31, 1f                    \n"
          "  " ASM_S     " $31, (%[rsz] * 0)($29)     \n"
          "  " ASM_LA    " $1, switchin_context_label \n"
          "  move $29, %[new_sp]                      \n"
          "  jr $1                                    \n"
          "    " ASM_L   " $31, (%[rsz] * 0)($29)     \n" // delay slot, load ra from new stack
          "1:                                         \n"
          "  " ASM_L " $28, (%[rsz] * (%[algn]+4))($29)     \n"
          "  " ASM_L " $30, (%[rsz] * (%[algn]+5))($29)     \n"
          "  " ASM_ADDIU " $29, $29, (%[rsz] * (%[algn]+6)) \n"
          ".set pop                                   \n"
          : [old_sp]   "+r" (_old_sp),
            [new_sp]   "+r" (_new_sp),
            [old_this] "+r" (_old_this),
            [new_this] "+r" (_new_this)
          : [rsz]"i"(ASM_WORD_BYTES),
#ifdef __mips64
            [algn]"i"(0)
#else
            [algn]"i"(1)
#endif
          : "$2",  "$3",  "$8",  "$9",  "$10", "$11", "$12",
            "$13", "$14", "$15", "$16", "$17", "$18", "$19", "$20", "$21",
            "$22", "$23", "$24", "$25", "$31", "memory"
#ifndef CONFIG_CPU_MIPSR6
            , "lo", "hi"
#endif
      );
    }
}

/** Thread context switchin.  Called on every re-activation of a
    thread (switch_exec()).  This method is public only because it is 
    called by assembly routine cpu_switchto via its assembly label
    switchin_context_label.  */
IMPLEMENT
void Context::switchin_context(Context *from)
{
  assert (this == current());
  assert (state() & Thread_ready_mask);

  from->handle_lock_holder_preemption();

  Space *spc = vcpu_aware_space();
  if (!switchin_guest_context(spc))
    // switch to our page directory if necessary
    spc->switchin_context(0);

  // load new kernel-entry SP into ErrorEPC as we use this
  // in exception entries from user mode
  Mips::mtc0(reinterpret_cast<Address>(regs() + 1), Mips::Cp0_err_epc);
}

// -------------------------------------------------
INTERFACE [mips && mips_vz]:

#include "vz.h"

// -------------------------------------------------
IMPLEMENTATION [mips && mips_vz]:

#include "vz.h"

PRIVATE inline FIASCO_FLATTEN
bool
Context::switchin_guest_context(Space *spc)
{
  bool const guest_context = (state() & Thread_vcpu_user) && (regs()->status & (1 << 3));
  if (EXPECT_FALSE(guest_context)
      && EXPECT_TRUE(home_cpu() == get_current_cpu()))
    {
      if (!(state() & Thread_vcpu_vz_owner))
        {
          auto &owner = Vz::owner.cpu(get_current_cpu());
          if (owner.ctxt)
            owner.ctxt->vz_save_state(owner.guest_id);

          owner.ctxt = this;
          owner.guest_id = spc->switchin_guest_context();
          vz_load_state(owner.guest_id);
        }
      else
        {
          [[maybe_unused]] unsigned guest_id = spc->switchin_guest_context();
#ifndef NDEBUG
          auto &owner = Vz::owner.cpu(get_current_cpu());
          assert (owner.ctxt == this);
          assert (static_cast<unsigned>(owner.guest_id) == guest_id);
#endif
        }
      return true;
    }

  return false;
}

PUBLIC static inline NEEDS["vz.h"]
Vz::State *
Context::vm_state(Vcpu_state *vs)
{ return offset_cast<Vz::State *>(vs, Config::Ext_vcpu_state_offset); }

IMPLEMENT_OVERRIDE inline
void
Context::copy_and_sanitize_trap_state(Trap_state *dst,
                                      Trap_state const *src) const
{
  *dst = access_once(src);
  dst->status &= Cp0_status::ST_USER_MASK;
  dst->status |= Cp0_status::ST_USER_MUST_SET;
  if ((state() & Thread_ext_vcpu_enabled) && (src->status & (1 << 3)))
    dst->status |= 1 << 3;
}

PUBLIC inline
void FIASCO_FLATTEN
Context::vz_save_state(int guest_id)
{
  vm_state(vcpu_state().kern())->save_full(guest_id);
  state_del_dirty(Thread_vcpu_vz_owner);
}

PUBLIC inline
void FIASCO_FLATTEN
Context::vz_load_state(int guest_id)
{
  auto *vm = vm_state(vcpu_state().kern());
  vm->load_full(guest_id);
  // mark the VM state as dirty as we execute the VM now
  vm->current_cp0_map = 0;
  state_add_dirty(Thread_vcpu_vz_owner);
}

IMPLEMENT_OVERRIDE inline
void Context::vcpu_pv_switch_to_kernel(Vcpu_state *vs, bool current)
{
  if (!current)
    return;

  auto st = state();
  if (!(st & Thread_ext_vcpu_enabled))
    return;

  auto *r = regs();
  if (EXPECT_FALSE(!(r->status & (1UL << 3))))
    return; // not coming from guest

  vm_state(vs)->save_on_exit(r->cause);
  // LOG_MSG_3VAL(this, "svz", (Mword)s, 0, 0);
}

IMPLEMENT_OVERRIDE
void
Context::arch_vcpu_ext_shutdown()
{
  if (!(state() & Thread_ext_vcpu_enabled))
    return;

  auto &owner = Vz::owner.current();
  if (owner.ctxt != this)
    return;

  // If we have a shared guest/root JTLB or VTLB we reset Guest.Wired to 0
  // to allow free use of all TLB entries for root mode.
  Mips::mtgc0_32(0, Mips::Cp0_wired);
  owner.ctxt = 0;
}


// -------------------------------------------------
IMPLEMENTATION [mips && !mips_vz]:

PRIVATE inline FIASCO_FLATTEN
bool
Context::switchin_guest_context(Space *)
{ return false; }

PUBLIC inline
void FIASCO_FLATTEN
Context::vz_save_state(int) {}

PUBLIC inline
void FIASCO_FLATTEN
Context::vz_load_state(int) {}

