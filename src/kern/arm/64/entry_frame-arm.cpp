INTERFACE [arm && 64bit]:

#include "l4_types.h"
#include "types.h"
#include "processor.h"

class Syscall_frame
{
private:
  Mword r[5];

public:
  void from(Mword id)
  { r[1] = id; }

  Mword from_spec() const
  { return r[1]; }

   L4_obj_ref ref() const
  { return L4_obj_ref::from_raw(r[2]); }

  void ref(L4_obj_ref const &ref)
  { r[2] = ref.raw(); }

  L4_timeout_pair timeout() const
  { return L4_timeout_pair(r[3]); }

  void timeout(L4_timeout_pair const &to)
  { r[3] = to.raw(); }

  Utcb *utcb() const
  { return reinterpret_cast<Utcb*>(r[4]); }

  L4_msg_tag tag() const
  { return L4_msg_tag(r[0]); }

  void tag(L4_msg_tag const &tag)
  { r[0] = tag.raw(); }
};

class Entry_frame;

class Return_frame
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

  Mword sp() const
  { return usp; }

  void sp(Mword sp)
  { usp = sp; }

  Mword ip() const
  { return pc; }

  void ip(Mword pc)
  { this->pc = pc; }

  Syscall_frame *syscall_frame()
  { return reinterpret_cast<Syscall_frame *>(&r[0]); }

  Syscall_frame const *syscall_frame() const
  { return reinterpret_cast<Syscall_frame const *>(&r[0]); }

  static Entry_frame *to_entry_frame(Syscall_frame *sf)
  {
    // assume Entry_frame == Return_frame
    return reinterpret_cast<Entry_frame *>
      (  reinterpret_cast<char *>(sf)
       - reinterpret_cast<intptr_t>(&reinterpret_cast<Return_frame *>(0)->r[0]));
  }
};

class Entry_frame : public Return_frame {};

//------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && !cpu_virt]:

PUBLIC inline
bool
Return_frame::check_valid_user_psr() const
{ return (pstate & Proc::Status_mode_mask) == 0x00; }

//------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && cpu_virt]:

PUBLIC inline
bool
Return_frame::check_valid_user_psr() const
{ return (pstate & 0x1c) != 0x08; }

// ------------------------------------------------------
IMPLEMENTATION [arm && 64bit && !cpu_virt]:

#include "processor.h"
#include "mem.h"

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

// ------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

#include <cstdio>
#include "processor.h"

PUBLIC inline NEEDS["processor.h"]
void
Return_frame::psr_set_mode(unsigned char m)
{
  pstate = (pstate & ~Proc::Status_mode_mask) | m;
}

PUBLIC inline NEEDS[Return_frame::check_valid_user_psr]
Mword
Return_frame::ip_syscall_user() const
{
  return check_valid_user_psr()
    ? r[30] // Assuming user entered the kernel by __l4_sys_syscall().
    : ip(); // Kernel IP.
}

PUBLIC
void
Syscall_frame::dump()
{
  printf(" R0: %08lx  R1: %08lx  R2: %08lx  R3: %08lx\n",
         r[0], r[1], r[2], r[3]);
  printf(" R4: %08lx\n", r[4]);
}


