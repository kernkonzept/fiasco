/*
 * Fiasco Kernel-Entry Frame-Layout Code for RISC-V
 */

INTERFACE [riscv]:

#include "cpu.h"
#include "types.h"
#include "warn.h"

class Syscall_frame
{
private:
  Mword a0;
  Mword a1;
  Mword a2;
  Mword a3;

public:
  void from(Mword id)
  { a3 = id; }

  Mword from_spec() const
  { return a3; }

  L4_obj_ref ref() const
  { return L4_obj_ref::from_raw(a1); }

  void ref(L4_obj_ref const &ref)
  { a1 = ref.raw(); }

  L4_timeout_pair timeout() const
  { return L4_timeout_pair(a2); }

  void timeout(L4_timeout_pair const &to)
  { a2 = to.raw(); }

  Utcb *utcb() const
  { return nullptr; }

  L4_msg_tag tag() const
  { return L4_msg_tag(a0); }

  void tag(L4_msg_tag const &tag)
  { a0 = tag.raw(); }
};

class Entry_frame;

class Return_frame
{
public:
  Mword eret_work;

  union
  {
    Mword regs[31];
    struct
    {
      Mword ra;
      Mword _sp;
      Mword gp;
      Mword tp;
      Mword t0, t1, t2;
      Mword s0, s1;
      Mword a0, a1, a2, a3, a4, a5, a6, a7;
      Mword s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
      Mword t3, t4, t5, t6;
    };
  };

  Mword _pc;
  Mword status;
  Mword cause;
  Mword tval;

  enum Exception_code : Mword
  {
    // Designated for custom use 0x18 - 0x1f
    Ec_l4_ipc_upcall          = 0x18,
    Ec_l4_exregs_exception    = 0x19,
    Ec_l4_debug_ipi           = 0x1a,
    Ec_l4_alien_after_syscall = 0x1b,
  };

public:
  Mword ip() const
  { return _pc; }

  Mword ip_syscall_user() const
  { return _pc; }

  void ip(Mword pc)
  { _pc = pc; }

  Mword sp() const
  { return _sp; }

  void sp(Mword sp)
  { _sp = sp; }

  Mword arg0() const
  { return a0; }

  void arg0(Mword arg)
  { a0 = arg; }

  bool user_mode() const
  { return !(status & Cpu::Sstatus_spp); }

  void user_mode(bool enable)
  {
    if (enable)
      status &= ~Cpu::Sstatus_spp;
    else
      status |= Cpu::Sstatus_spp;
  }

  bool interrupts_enabled() const
  { return status & Cpu::Sstatus_spie; }

  Syscall_frame *syscall_frame()
  { return reinterpret_cast<Syscall_frame *>(&a0); }

  Syscall_frame const *syscall_frame() const
  { return reinterpret_cast<Syscall_frame const *>(&a0); }
};

class Entry_frame : public Return_frame
{
public:
  static Entry_frame *to_entry_frame(Syscall_frame *sf)
  {
    // assume Entry_frame == Return_frame
    return reinterpret_cast<Entry_frame *>
      (  reinterpret_cast<char *>(sf)
       - reinterpret_cast<intptr_t>(&reinterpret_cast<Return_frame *>(0)->a0));
  }

  void dump(bool extended = true) const;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "cpu.h"
#include "mem.h"

static_assert(sizeof(Entry_frame) == Cpu::stack_align(sizeof(Entry_frame)),
              "Size of entry frame is not a multiple of stack alignment.");

PUBLIC inline NEEDS["mem.h"]
void
Entry_frame::copy_and_sanitize(Entry_frame const *src)
{
  // Omit eret_work, cause, tval
  Mem::memcpy_mwords(&regs[0], &src->regs[0], sizeof(regs) / sizeof(regs[0]));
  _pc = src->_pc;
  // Sanitize status register.
  status = access_once(&src->status) & Cpu::Sstatus_user_mask;
  status |= Cpu::Sstatus_user_default;
}

IMPLEMENT
void
Entry_frame::dump(bool extended) const
{
  printf("pc=" L4_MWORD_FMT " status=" L4_MWORD_FMT "\n"
         "cause=" L4_MWORD_FMT " tval=" L4_MWORD_FMT "\n"
         "ra=" L4_MWORD_FMT " sp=" L4_MWORD_FMT "\n"
         "gp=" L4_MWORD_FMT " tp=" L4_MWORD_FMT "\n"
         "mode=%s\n",
         ip(), status, cause, tval, ra, _sp, gp, tp,
         user_mode() ? "user" : "kernel");

  if (extended)
    {
      printf(
        "a0=" L4_MWORD_FMT "  a1=" L4_MWORD_FMT "  a2=" L4_MWORD_FMT "\n"
        "a3=" L4_MWORD_FMT "  a4=" L4_MWORD_FMT "  a5=" L4_MWORD_FMT "\n"
        "a6=" L4_MWORD_FMT "  a7=" L4_MWORD_FMT "\n"
        "t0=" L4_MWORD_FMT "  t1=" L4_MWORD_FMT "  t2=" L4_MWORD_FMT "\n"
        "t3=" L4_MWORD_FMT "  t4=" L4_MWORD_FMT "  t5=" L4_MWORD_FMT "\n"
        "t6=" L4_MWORD_FMT "\n"
        "s0=" L4_MWORD_FMT "  s1=" L4_MWORD_FMT "  s2=" L4_MWORD_FMT "\n"
        "s3=" L4_MWORD_FMT "  s4=" L4_MWORD_FMT "  s5=" L4_MWORD_FMT "\n"
        "s6=" L4_MWORD_FMT "  s7=" L4_MWORD_FMT "  s8=" L4_MWORD_FMT "\n"
        "s9=" L4_MWORD_FMT " s10=" L4_MWORD_FMT " s11=" L4_MWORD_FMT "\n",
        a0, a1, a2, a3, a4, a5, a6, a7, t0, t1, t2, t3, t4, t5, t6,
        s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11);
    }
}
