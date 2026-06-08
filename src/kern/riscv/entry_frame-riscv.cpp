INTERFACE [riscv]:

#include "cpu.h"
#include "types.h"
#include "warn.h"

EXTENSION class Syscall_frame
{
private:
  Mword a0;
  Mword a1;
  Mword a2;
  Mword a3;
};

EXTENSION class Return_frame
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
};

EXTENSION class Entry_frame
{
public:
  void dump(bool extended = true) const;
};

//---------------------------------------------------------------------------
INTERFACE [riscv && !cpu_virt]:

EXTENSION class Return_frame
{
public:
  Mword _reserved; // hstatus if virtualization is enabled.
};

//---------------------------------------------------------------------------
INTERFACE [riscv && cpu_virt]:

EXTENSION class Return_frame
{
public:
  Mword hstatus;

  bool virt_mode() const
  { return hstatus & Cpu::Hstatus_spv; }

  void virt_mode(bool enable)
  {
    if (enable)
      hstatus |= Cpu::Hstatus_spv;
    else
      hstatus &= ~Cpu::Hstatus_spv;
  }

  /// Control usage of hypervisor load/store instructions
  void allow_vmm_hyp_load_store(bool allow)
  {
    if (allow)
      hstatus |= Cpu::Hstatus_hu;
    else
      hstatus &= ~Cpu::Hstatus_hu;
  }
};

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "cpu.h"
#include "mem.h"

IMPLEMENT inline
void
Syscall_frame::from(Mword id)
{ a3 = id; }

IMPLEMENT inline
Mword
Syscall_frame::from_spec() const
  { return a3; }

IMPLEMENT inline
L4_obj_ref
Syscall_frame::ref() const
{ return L4_obj_ref::from_raw(a1); }

IMPLEMENT inline
void
Syscall_frame::ref(L4_obj_ref const &ref)
{ a1 = ref.raw(); }

IMPLEMENT inline
L4_timeout_pair
Syscall_frame::timeout() const
{ return L4_timeout_pair(a2); }

IMPLEMENT inline
void
Syscall_frame::timeout(L4_timeout_pair const &to)
{ a2 = to.raw(); }

IMPLEMENT inline
Utcb *
Syscall_frame::utcb() const
{ return nullptr; }

IMPLEMENT inline
L4_msg_tag
Syscall_frame::tag() const
{ return L4_msg_tag(a0); }

IMPLEMENT inline
void
Syscall_frame::tag(L4_msg_tag const &tag)
{ a0 = tag.raw(); }


IMPLEMENT inline
Mword
Return_frame::ip() const
{ return _pc; }

IMPLEMENT inline
Mword
Return_frame::ip_syscall_user() const
{ return _pc; }

IMPLEMENT inline
void
Return_frame::ip(Mword new_ip)
{ _pc = new_ip; }

IMPLEMENT inline
Mword
Return_frame::sp() const
{ return _sp; }

IMPLEMENT inline
void
Return_frame::sp(Mword new_sp)
{ _sp = new_sp; }

IMPLEMENT inline
void
Return_frame::disable_continuation()
{ eret_work = 0; }


IMPLEMENT static inline
Entry_frame *
Entry_frame::to_entry_frame(Syscall_frame *sf)
{
  // assume Entry_frame == Return_frame
  return reinterpret_cast<Entry_frame *>
      (reinterpret_cast<char *>(sf) - offsetof(Return_frame, a0));
}

IMPLEMENT inline
Syscall_frame *
Entry_frame::syscall_frame()
{ return reinterpret_cast<Syscall_frame *>(&a0); }

IMPLEMENT inline
Syscall_frame const *
Entry_frame::syscall_frame() const
{ return reinterpret_cast<Syscall_frame const *>(&a0); }

PUBLIC inline NEEDS["mem.h"]
void
Entry_frame::copy_and_sanitize(Entry_frame const *src)
{
  // Omit eret_work, cause, tval
  Mem::memcpy_mwords(&regs[0], &src->regs[0], cxx::size(regs));
  _pc = src->_pc;

  // Sanitize status register.
  status = access_once(&src->status) & Cpu::Sstatus_user_mask;
  status |= Cpu::Sstatus_user_default;

  copy_and_sanitize_hstatus(src);
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

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv && !cpu_virt]:

PUBLIC inline void Entry_frame::init_hstatus() {}
PUBLIC inline void Entry_frame::copy_hstatus(Return_frame const *) {}
PUBLIC inline void Entry_frame::copy_and_sanitize_hstatus(Entry_frame const *) {}

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt]:

PUBLIC inline
void
Entry_frame::init_hstatus()
{ hstatus = Cpu::Hstatus_user_default; }

PUBLIC inline
void
Entry_frame::copy_hstatus(Return_frame const *src)
{ hstatus = src->hstatus; }

PUBLIC inline
void
Entry_frame::copy_and_sanitize_hstatus(Entry_frame const *src)
{
  // Sanitize hstatus register.
  hstatus = access_once(&src->hstatus) & Cpu::Hstatus_user_mask;
  hstatus |= Cpu::Hstatus_user_default;
}
