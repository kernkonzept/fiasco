IMPLEMENTATION [mips]:

#include <cstdio>
#include <cassert>
#include "alternatives.h"
#include "asm_mips.h"
#include "cp0_status.h"
#include "trap_state.h"
#include "processor.h"
#include "types.h"

DEFINE_PER_CPU Per_cpu<Thread::Dbg_stack> Thread::dbg_stack;

PUBLIC inline NEEDS ["trap_state.h"]
bool
Thread::send_exception_arch(Trap_state *)
{ return true; }

PROTECTED inline
int
Thread::sys_control_arch(Utcb *)
{
  return 0;
}

PRIVATE inline
int
Thread::cache_op(unsigned op, Address start, Address end)
{
  jmp_buf pf_recovery;
  if (setjmp(pf_recovery) != 0)
    {
      recover_jmp_buf(0);
      return -L4_err::EFault;
    }

  recover_jmp_buf(&pf_recovery);

  switch (op)
    {
    case 0x22:
      // this is invalidate only, however we do a flush
      // otherwise we would need to do a flush for incomplete
      // cachelines at the start end end of the range
    case 0x20:
      Mem_unit::dcache_flush(start, end);
      break;
    case 0x21:
      Mem_unit::dcache_clean(start, end);
      break;
    }

  recover_jmp_buf(0);
  return 0;
}


PROTECTED inline NEEDS["processor.h", Thread::sys_vz_save_state,
                       Thread::cache_op]
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb *utcb)
{
  switch (unsigned op = access_once(&utcb->values[0]) & Opcode_mask)
    {
    case 0x10: // set ULR op-code
      if (tag.words() < 2)
        return commit_result(-L4_err::EMsgtooshort);

      _ulr = access_once(&utcb->values[1]);
      if (current() == this)
        Proc::set_ulr(_ulr);
      return Kobject_iface::commit_result(0);

    case 0x14:
      return commit_result(sys_vz_save_state(tag, utcb));

    case 0x20:
    case 0x21:
    case 0x22:
      if (tag.words() < 3)
        return commit_result(-L4_err::EMsgtooshort);

      return commit_result(cache_op(op, access_once(&utcb->values[1]),
                                    access_once(&utcb->values[2])));

    default:
      return commit_result(-L4_err::ENosys);
    }
}

IMPLEMENT inline Mword Thread::user_sp() const    { return regs()->sp(); }
IMPLEMENT inline void  Thread::user_sp(Mword sp)  { regs()->sp(sp); }
IMPLEMENT inline Mword Thread::user_flags() const { return regs()->status; }
IMPLEMENT inline Mword Thread::user_ip() const    { return regs()->ip(); }
IMPLEMENT inline void  Thread::user_ip(Mword ip)  { regs()->ip(ip); }

/** Constructor.
    @param id user-visible thread ID of the sender
    @param init_prio initial priority
    @param mcp thread's maximum controlled priority
    @post state() != 0
 */
IMPLEMENT
Thread::Thread(Ram_quota *q)
: Sender(0),
  _pager(Thread_ptr::Invalid),
  _exc_handler(Thread_ptr::Invalid),
  _quota(q),
  _del_observer(0)
{

  assert(state(false) == 0);

  inc_ref();
  _space.space(Kernel_task::kernel_task());

  if (Config::Stack_depth)
    std::memset((char*)this + sizeof(Thread), '5',
                Thread::Size - sizeof(Thread) - 64);

  // set a magic value -- we use it later to verify the stack hasn't
  // been overrun
  _magic = magic;
  _recover_jmpbuf = 0;
  _timeout = 0;

  prepare_switch_to(&user_invoke);

  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  memset(r, 0, sizeof(*r));
  r->status = Cp0_status::status_eret_to_user_ei(Cp0_status::read());

  state_add_dirty(Thread_dead, false);
  // ok, we're ready to go!
}

IMPLEMENT inline
bool
Thread::pagein_tcb_request(Return_frame *)
{
  assert(false);
  return false;
}

// ERET to user mode
IMPLEMENT
void
Thread::user_invoke()
{
  user_invoke_generic();
  assert(current()->state() & Thread_ready);
  auto ts = current()->regs();

  Proc::cli();

  ts->r[4] = 0;

  if (EXPECT_FALSE(current_thread()->mem_space()->is_sigma0()))
    ts->r[4] = Mem_layout::pmem_to_phys(Kip::k());

  // FIXME: do we really need this or should the user be
  // responsible for that
  //Mem_op::cache()->icache_invalidate_all();

    {
      extern char ret_from_user_invoke[];
      Mword register a0 __asm__("a0") = (Mword)ts;
      Mword register ra __asm__("ra") = (Mword)ret_from_user_invoke;
      __asm__ __volatile__ (
          ASM_ADDIU "  $sp, %[ts], -%[cfs]   \n"
          "jr          %[ra]                 \n"
          :
          : [ra] "r" (ra),
            [ts] "r" (a0),
            [cfs] "i" (ASM_WORD_BYTES * ASM_NARGSAVE));
    }

  __builtin_unreachable();
  panic("should never be reached");
  while (1)
    {
      current()->state_del(Thread_ready);
      current()->schedule();
    };

  // never returns
}

IMPLEMENT inline NEEDS["space.h", "types.h", "config.h"]
bool Thread::handle_sigma0_page_fault(Address pfa)
{
  return mem_space()
    ->v_insert(Mem_space::Phys_addr((pfa & Config::SUPERPAGE_MASK)),
               Virt_addr(pfa & Config::SUPERPAGE_MASK),
               Virt_order(Config::SUPERPAGE_SHIFT) /*mem_space()->largest_page_size()*/,
               Mem_space::Attr(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
}

PROTECTED inline
int
Thread::do_trigger_exception(Entry_frame *r, void *ret_handler)
{
  if (!_exc_cont.valid(r))
    {
      _exc_cont.activate(r, ret_handler);
      return 1;
    }
  return 0;
}

PRIVATE static
void
Thread::print_page_fault_error(Mword e)
{
  printf("%s (%ld), %s(%c) (%lx)",
         Trap_state::exc_code_to_str(e), (e >> 2) & 0x1f,
         PF::is_usermode_error(e) ? "user" : "kernel",
         PF::is_read_error(e) ? 'r' : 'w', e);
}

PUBLIC static inline NEEDS["alternatives.h"]
void
Thread::save_bad_instr(Trap_state *ts)
{
  asm volatile (ALTERNATIVE_INSN(
        "move %0, $0",
        "mfc0 %0, $8, 1",
        0x8 /* FEATURE_BI */)
      : "=r"(ts->bad_instr));
  asm volatile (ALTERNATIVE_INSN(
        "move %0, $0",
        "mfc0 %0, $8, 2",
        0x10 /* FEATURE_BP */)
      : "=r"(ts->bad_instr_p));
}

PUBLIC inline NEEDS[Thread::send_exception, Thread::save_bad_instr]
int
Thread::handle_slow_trap(Trap_state::Cause cause, Trap_state *ts,
                         bool save_bad = true)
{
  bool const from_user = ts->status & Trap_state::S_ksu;

  if (save_bad && from_user)
    save_bad_instr(ts);

  if (from_user && _space.user_mode() && send_exception(ts))
    return 0;

  if (EXPECT_FALSE(!from_user))
    return Thread::call_nested_trap_handler(ts);

  // FIXME: HACK trap 'break' to JDB even from user mode
  if (cause.exc_code() == 9)
    return Thread::call_nested_trap_handler(ts);

  if (send_exception(ts))
    return 0;

  // FIXME: should probably kill the thread and schedule here,
  //        enter JDB for now
  return Thread::call_nested_trap_handler(ts);
}


extern "C" FIASCO_FASTCALL FIASCO_FLATTEN
void
thread_handle_trap(Mword cause, Trap_state *ts)
{
  Thread *ct = current_thread();
  LOG_TRAP_CN(ct, cause);
  if (ct->handle_slow_trap(cause, ts))
    ct->halt();
}

extern "C" void
handle_fpu_trap(Trap_state::Cause cause, Trap_state *ts)
{
  Thread *ct = current_thread();
  LOG_TRAP_CN(ct, cause.raw);
  if (!ct->switchin_fpu() && ct->handle_slow_trap(cause, ts))
    ct->halt();
}

extern "C" FIASCO_FASTCALL FIASCO_FLATTEN
void
thread_handle_tlb_fault(Mword cause, Trap_state *ts, Mword pfa)
{
  Thread *t = current_thread();
  Space *s = t->vcpu_aware_space();
  LOG_TRACE("TLB miss", "tlb", t, Tb_entry_pf,
      l->set(t, ts->epc, pfa, cause, s);
  );

  assert (s);
  // Uses reserved Cause bit 0 (see exception.S) to flag a TLB miss
  bool need_probe = !(cause & 1);
  bool guest = ts->status & (1 << 3);

  if (EXPECT_FALSE(!s->add_tlb_entry(Virt_addr(pfa), !PF::is_read_error(cause), need_probe, guest)))
    {
      // TODO: Think about t->state_del(Thread_cancel); and sync with
      // at least ARM
      Thread::save_bad_instr(ts);
      if (t->vcpu_pagefault(pfa, cause, ts->epc))
        return;

      if (!t->handle_page_fault(pfa, cause, ts->epc, ts))
        {
          if (t->handle_slow_trap(cause, ts, false))
            t->halt();
        }

      s->add_tlb_entry(Virt_addr(pfa), !PF::is_read_error(cause), true, guest);
    }
}

// handle exceptions that MUST usually never happen
extern "C" FIASCO_FASTCALL FIASCO_FLATTEN
void
thread_unhandled_trap(Mword, Trap_state *ts)
{
  if (Thread::call_nested_trap_handler(ts))
    current_thread()->halt();
}


//
// Public services
//
PUBLIC inline NEEDS[<cassert>, "cp0_status.h"]
void FIASCO_NORETURN
Thread::fast_return_to_user(Mword ip, Mword sp, void *arg)
{
  assert (cpu_lock.test());
  assert (current() == this);

  {
    Mword register a0 __asm__("a0") = (Mword)arg;
    Mword register t9 __asm__("t9") = (Mword)ip;
    asm volatile
      (".set push                     \n"
       ".set noat                     \n"
       "  mfc0  $1, $12               \n"
       "  ins   $1, %[status], 0, 8   \n"
       "  move  $29, %[sp]            \n"
       "  " ASM_MTC0 "  %[ip], $14    \n"
       "  mtc0  $1, $12               \n"
       "  ehb                         \n"
       "  eret                        \n"
       ".set pop                      \n"
       : : [status] "r" (Cp0_status::ST_USER_DEFAULT),
           [ip] "r" (t9), [sp] "r" (sp), [arg] "r" (a0)
      );
  }

  panic("__builtin_trap()");
}

extern "C" void leave_by_vcpu_upcall()
{
  Thread *c = current_thread();
  c->regs()->r[0] = 0; // reset continuation
  Vcpu_state *vcpu = c->vcpu_state().access();
  vcpu->_regs.s = *nonull_static_cast<Trap_state*>(c->regs());
  c->fast_return_to_user(vcpu->_entry_ip, vcpu->_entry_sp, c->vcpu_state().usr().get());
}

PRIVATE static inline
void
Thread::save_fpu_state_to_utcb(Trap_state *, Utcb *)
{}

PRIVATE static inline
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // only a complete state will be used.
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = (Trap_state*)rcv->_utcb_handler;
  Utcb *snd_utcb = snd->utcb().access();

  Trex const *r = reinterpret_cast<Trex const *>(snd_utcb->values);
  // this skips the eret/continuation work already
  ts->copy_and_sanitize(&r->s);
  rcv->_ulr = access_once(&r->ulr);
  if (rcv == current())
    Proc::set_ulr(rcv->_ulr);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::W()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PRIVATE static inline NEEDS[Thread::save_fpu_state_to_utcb, "trap_state.h"]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = (Trap_state*)snd->_utcb_handler;

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *r = reinterpret_cast<Trex *>(rcv_utcb->values);
    r->s = *ts;
    r->ulr = snd->_ulr;

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::W()))
      {
        snd->save_fpu_state_to_utcb(ts, rcv_utcb);
        snd->transfer_fpu(rcv);
      }
    __asm__ __volatile__ ("" : : "m"(*r));
  }
  return true;
}

//----------------------------------------------------------
IMPLEMENTATION [mips && !mips_vz]:

PRIVATE inline
int
Thread::sys_vz_save_state(L4_msg_tag, Utcb *)
{ return -L4_err::ENosys; }

//----------------------------------------------------------
IMPLEMENTATION [mips && mips_vz]:

#include "cpu.h"
#include "vz.h"

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h"]
bool
Thread::arch_ext_vcpu_enabled()
{ return Cpu::options.vz(); }

IMPLEMENT_OVERRIDE
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext)
{
  vcpu_state->version = Vcpu_arch_version;

  if (!ext || (state(false) & Thread_ext_vcpu_enabled))
    return;

  Vz::State *v = vm_state(vcpu_state);
  v->init();
}

PRIVATE inline
int
Thread::sys_vz_save_state(L4_msg_tag tag, Utcb *utcb)
{
  if (tag.words() < 2)
    return -L4_err::EMsgtooshort;

  if (!(state() & Thread_ext_vcpu_enabled))
    return -L4_err::EInval;

  if (current() != this)
    return -L4_err::EInval;

  auto *v = vm_state(vcpu_state().kern());

  // must be saved already
  if (!(state() & Thread_vcpu_vz_owner))
    {
      // update the cause TI bit in this case
      if (utcb->values[1] & Vz::State::M_cause)
        v->update_cause_ti();
      return 0;
    }

  // always read the cause register when requested by the VMM
  if (utcb->values[1] & Vz::State::M_cause)
    v->current_cp0_map &= ~Vz::State::M_cause;

  // we have a bitmap in utcb->values[1] which state to save, however
  // we save the full state for now.
  v->save_full(Vz::owner.current().guest_id);
  return 0;
}

static bool
thread_guest_tlb_probe(Mword *pfa)
{
  using namespace Mips;
  Mword gindex;
  Mword gentryhi;
  Mword gctl1 = Mem_unit::vz_guest_ctl1();
  Mem_unit::set_vz_guest_rid(gctl1, gctl1 & 0xff);
  mfgc0_32(&gindex, Cp0_index);
  mfgc0(&gentryhi, Cp0_entry_hi);
  mtgc0((*pfa & ~0x3ff) | (gentryhi & 0x3ff), Cp0_entry_hi);
  asm volatile (".set push; .set virt; ehb; tlbgp; ehb; .set pop");
  Mword index;
  mfgc0_32(&index, Cp0_index);
  if (index & (1UL << 31))
    {
      mtgc0_32(gindex, Cp0_index);
      mtgc0(gentryhi, Cp0_entry_hi);
      ehb();

      return false;
    }

  Mword gelo[2];
  Mword gpmask;
  mfgc0(&gelo[0], Cp0_entry_lo1);
  mfgc0(&gelo[1], Cp0_entry_lo2);
  mfgc0_32(&gpmask, Cp0_page_mask);
  asm volatile (".set push; .set virt; tlbgr; ehb; .set pop");

  Mword entry;
  Mword mask;
  mfgc0_32(&mask, Cp0_page_mask);
  mask = ((mask | 0x1fff) + 1) >> 1;
  if (*pfa & mask)
    mfgc0(&entry, Cp0_entry_lo2);
  else
    mfgc0(&entry, Cp0_entry_lo1);

  *pfa = ((entry & 0x3fffffc0) << 6) | (*pfa & 0xfff);

  mtgc0(gelo[0], Cp0_entry_lo1);
  mtgc0(gelo[1], Cp0_entry_lo2);
  mtgc0_32(gpmask, Cp0_page_mask);
  mtgc0_32(gindex, Cp0_index);
  mtgc0(gentryhi, Cp0_entry_hi);
  ehb();
  return true;
}

inline bool
thread_translate_gva_32bit_segments(Mword *pfa)
{
  Mword cfg;
#define ASM_SEGCTL_ALT(val, reg, feature) \
  asm (ALTERNATIVE_INSN(                  \
        "li %0, "#val,                    \
        ".set push; .set virt\n\t"        \
        "mfgc0 %0, $5, "#reg"\n\t"        \
        ".set pop", (1 << 8)) : "=r" (cfg))


  if (*pfa & (1UL << 31))
    {
      if (*pfa & (1UL << 30))
        ASM_SEGCTL_ALT(0x00200010, 2, 1 << 8);
      else
        ASM_SEGCTL_ALT(0x00030002, 3, 1 << 8);

      if (*pfa & (1UL << 29))
        cfg &= 0xffff;
      else
        cfg = (cfg >> 16) & 0xffff;
    }
  else
    {
      ASM_SEGCTL_ALT(0x04330033, 4, 1 << 8);

      if (*pfa & (1UL << 30))
        cfg &= 0xffff;
      else
        cfg = (cfg >> 16) & 0xffff;
    }

#undef ASM_SEGCTL_ALT

  unsigned am = (cfg >> 4) & 0x7;
  bool is_mapped = false;
  switch (am)
    {
    case 0: /*UK*/    break;
    case 1: /*MK*/    is_mapped = true; break;
    case 2: /*MSK*/   is_mapped = true; break;
    case 3: /*MUSK*/  is_mapped = true; break;
    case 4: /*MUSUK*/
      {
        Mword gstatus = mfgc0_32(Mips::Cp0_status);
        unsigned plevel = (gstatus >> 3) & 0x3;
        if (gstatus & 0x6)
          plevel = 0;
        is_mapped = (plevel != 0);
        break;
      }
    case 5: /*USK*/  break;
    case 7: /*UUSK*/ break;
    default: is_mapped = true; break;
    }

  if (!is_mapped)
    {
      if (*pfa & (1UL << 31))
        *pfa = (*pfa & 0x1fffffff) | ((cfg >> 9) << 29);
      else
        *pfa = (*pfa & 0x3fffffff) | ((cfg >> 10) << 30);
    }

  return is_mapped;
}

static void FIASCO_FLATTEN
thread_handle_gva_tlb_fault(Mword cause, Trap_state *ts, Mword pfa)
{
  auto *t = current_thread();

  bool is_mapped = thread_translate_gva(&pfa);

  if (is_mapped && !thread_guest_tlb_probe(&pfa))
    {
      if (t->handle_slow_trap(cause, ts, true))
        t->halt();
      return;
    }

  Space *s = t->vcpu_aware_space();
  if (EXPECT_TRUE(s->add_tlb_entry(Virt_addr(pfa), !PF::is_read_error(cause), true, true)))
    return;

  Thread::save_bad_instr(ts);

  Vcpu_state *vcpu = t->vcpu_state().access();
  t->spill_user_state();
  t->vcpu_enter_kernel_mode(vcpu);
  vcpu->_regs.s = *ts;
  vcpu->_regs.s.bad_v_addr = pfa;
  Context::vm_state(vcpu)->ctl_0 = (Context::vm_state(vcpu)->ctl_0 & ~0x3c) | (10 << 2);
  t->fast_return_to_user(vcpu->_entry_ip, vcpu->_sp, t->vcpu_state().usr().get());
}

extern "C" FIASCO_FASTCALL FIASCO_FLATTEN
void
thread_handle_tlb_fault_vz(Mword cause, Trap_state *ts, Mword pfa)
{
  if (ts->status & (1 << 3))
    {
      Mword c = Mips::mfc0_32(Mips::Cp0_guest_ctl_0);
      if (EXPECT_FALSE(((c >> 2) & 0x1f) == 8))
        {
          thread_handle_gva_tlb_fault(cause, ts, pfa);
          return;
        }
    }
  thread_handle_tlb_fault(cause, ts, pfa);
}

//----------------------------------------------------------
IMPLEMENTATION [mips && mips_vz && mips32]:

inline bool FIASCO_FLATTEN
thread_translate_gva(Mword *pfa)
{
  return thread_translate_gva_32bit_segments(pfa);
}

//----------------------------------------------------------
IMPLEMENTATION [mips && mips_vz && mips64]:

inline bool FIASCO_FLATTEN
thread_translate_gva(Mword *pfa)
{
  if (*pfa < (1UL << 31) || *pfa >= ~((1UL << 31) - 1))
    return thread_translate_gva_32bit_segments(pfa);

  if (*pfa >> 62 == 2) // xkphys
    {
      *pfa &= (1UL << 59) -1;
      return false;
    }

  return true;
}

// -----------------------------------------------------
IMPLEMENTATION [mips && mp]:

#include "ipi.h"
#include "mips_cpu_irqs.h"

class Thread_remote_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  {
    auto c = current_cpu();
    auto ipi = Ipi::ipis(c);
    Ipi::hw->ack_ipi(c);

    if (ipi->atomic_reset(Ipi::Request))
      Thread::handle_remote_requests_irq();

    if (ipi->atomic_reset(Ipi::Global_request))
      Thread::handle_global_remote_requests_irq();

    if (ipi->atomic_reset(Ipi::Debug))
      {
        // fake a trap-state for the nested_trap handler and set the cause to debug ipi
        Trap_state ts;
        Trap_state::Cause *cause = &reinterpret_cast<Trap_state::Cause &>(ts.cause);
        cause->bp_spec() = 2;
        cause->exc_code() = 9;
        Thread::call_nested_trap_handler(&ts);
      }
  }

  Thread_remote_irq()
  {
    if (Ipi::hw == nullptr)
      return;

    Ipi::hw->init_ipis(Cpu_number::boot_cpu(), this);
    set_hit(&handler_wrapper<Thread_remote_irq>);
    unmask();
  }

  void switch_mode(bool) {}
};

static Thread_remote_irq _ipiiii;

