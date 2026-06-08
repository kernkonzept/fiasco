INTERFACE [arm && 64bit]:

#include "l4_types.h"
#include "types.h"
#include "processor.h"

EXTENSION class Syscall_frame
{
private:
  Mword r[5];
};

EXTENSION class Return_frame
{
public:
  Mword eret_work;
  Mword r[31];
  Mword _ksp;
  union
  {
    Arm_esr esr;
    Mword error_code;
  };
  Mword pf_address;
  Mword usp;
  Mword pc;
  union
  {
    Mword pstate;
    Mword psr;
  };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

#include "processor.h"

IMPLEMENT inline
void
Syscall_frame::from(Mword id)
{ r[1] = id; }

IMPLEMENT inline
Mword
Syscall_frame::from_spec() const
{ return r[1]; }

IMPLEMENT inline
L4_obj_ref
Syscall_frame::ref() const
{ return L4_obj_ref::from_raw(r[2]); }

IMPLEMENT inline
void
Syscall_frame::ref(L4_obj_ref const &ref)
{ r[2] = ref.raw(); }

IMPLEMENT inline
L4_timeout_pair
Syscall_frame::timeout() const
{ return L4_timeout_pair(r[3]); }

IMPLEMENT inline
void
Syscall_frame::timeout(L4_timeout_pair const &to)
{ r[3] = to.raw(); }

IMPLEMENT inline
Utcb *
Syscall_frame::utcb() const
{ return reinterpret_cast<Utcb*>(r[4]); }

IMPLEMENT inline
L4_msg_tag
Syscall_frame::tag() const
{ return L4_msg_tag(r[0]); }

IMPLEMENT inline
void
Syscall_frame::tag(L4_msg_tag const &tag)
{ r[0] = tag.raw(); }


IMPLEMENT inline
Mword
Return_frame::sp() const
{ return usp; }

IMPLEMENT inline
void
Return_frame::sp(Mword new_sp)
{ usp = new_sp; }

IMPLEMENT inline
Mword
Return_frame::ip() const
{ return pc; }

IMPLEMENT inline
void
Return_frame::ip(Mword new_ip)
{ pc = new_ip; }

IMPLEMENT inline NEEDS[Return_frame::check_valid_user_psr]
Mword
Return_frame::ip_syscall_user() const
{
  return check_valid_user_psr()
    ? r[30] // Assuming user entered the kernel by __l4_sys_syscall().
    : ip(); // Kernel IP.
}

IMPLEMENT inline
void
Return_frame::disable_continuation()
{ eret_work = 0; }

PUBLIC inline NEEDS["processor.h"]
void
Return_frame::psr_set_mode(unsigned char m)
{ pstate = (pstate & ~Proc::Status_mode_mask) | m; }


IMPLEMENT static inline
Entry_frame *
Entry_frame::to_entry_frame(Syscall_frame *sf)
{
  // assume Entry_frame == Return_frame
  return reinterpret_cast<Entry_frame *>
    (reinterpret_cast<char *>(sf) - offsetof(Return_frame, r[0]));
}

IMPLEMENT inline
Syscall_frame *
Entry_frame::syscall_frame()
{ return reinterpret_cast<Syscall_frame *>(&r[0]); }

IMPLEMENT inline
Syscall_frame const *
Entry_frame::syscall_frame() const
{ return reinterpret_cast<Syscall_frame const *>(&r[0]); }

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && !cpu_virt]:

#include "processor.h"
#include "mem.h"

PUBLIC inline
bool
Return_frame::check_valid_user_psr() const
{ return (pstate & Proc::Status_mode_mask) == 0x00; }


PUBLIC inline NEEDS["processor.h", "mem.h"]
void
Entry_frame::copy_and_sanitize(Entry_frame const *src)
{
  // omit eret_work, ksp, esr, pf_address
  Mem::memcpy_mwords(&r[0], &src->r[0], 31);
  usp = src->usp;
  pc  = src->pc;
  pstate = access_once(&src->pstate);
  pstate &= ~(Mword{Proc::Status_mode_mask} | Mword{Proc::Status_interrupts_mask});
  pstate |= Mword{Proc::Status_mode_user} | Mword{Proc::Status_always_mask};
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && cpu_virt]:

PUBLIC inline
bool
Return_frame::check_valid_user_psr() const
{ return (pstate & 0x1c) != 0x08; }
