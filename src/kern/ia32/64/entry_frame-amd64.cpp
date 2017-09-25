/*
 * Fiasco Kernel-Entry Frame-Layout Code for x86-64
 */

INTERFACE[amd64]:

#include "types.h"

EXTENSION class Syscall_post_frame
{
  Mword    _r15;
  Mword    _r14;
  Mword    _r13;
  Mword    _r12;
  Mword    _r11;
  Mword    _r10;
  Mword     _r9;
  Mword     _r8;
};

EXTENSION class Syscall_frame
{
protected:
  Mword    _rdi;
  Mword    _rsi;
  Mword    _rbp;
  Mword    _reserved;
  Mword    _rbx;
  Mword    _rdx;
  Mword    _rcx;
  Mword    _rax;
};

EXTENSION class Syscall_pre_frame
{
  Mword    _reserved_1[2];
};

EXTENSION class Return_frame
{
private:
  Mword    _rip;
  Mword     _cs;
  Mword _rflags;
  Mword    _rsp;
  Mword     _ss;

public:
  enum { Pf_ax_offset = 2 };
};

IMPLEMENTATION[ux,amd64]:

//---------------------------------------------------------------------------
// basic Entry_frame methods for IA32
// 
#include "mem_layout.h"

IMPLEMENT inline
Address
Return_frame::ip() const
{ return _rip; }

IMPLEMENT inline NEEDS["mem_layout.h"]
Address
Return_frame::ip_syscall_page_user() const
{
  Address rip = ip();
  if ((rip & Mem_layout::Syscalls) == Mem_layout::Syscalls)
     rip = *(Mword*)sp();
  return rip;
}

IMPLEMENT inline
void
Return_frame::ip(Mword ip)
{
  // We have to consider a special case where we have to leave the kernel
  // with iret instead of sysexit: If the target thread entered the kernel
  // through sysenter, it would leave using sysexit. This is not possible
  // for two reasons: Firstly, the sysexit instruction needs special user-
  // land code to load the right value into the edx register (see user-
  // level sysenter bindings). And secondly, the sysexit instruction
  // decrements the user-level eip value by two to ensure that the fixup
  // code is executed. One solution without kernel support would be to add
  // the instructions "movl %ebp, %edx" just _before_ the code the target
  // eip is set to.
  if (cs() & 0x80)
    {
      // this cannot happen in Fiasco UX
      /* symbols from the assember entry code */
      extern Mword leave_from_syscall_by_iret;
      //extern Mword leave_alien_from_syscall_by_iret;
      //extern Mword ret_from_fast_alien_ipc;
      Mword **ret_from_disp_syscall = reinterpret_cast<Mword**>(static_cast<Entry_frame*>(this))-1;
      cs(cs() & ~0x80);
      // if (*ret_from_disp_syscall == &ret_from_fast_alien_ipc)
      //   *ret_from_disp_syscall = &leave_alien_from_sysenter_by_iret;
      //else
      *ret_from_disp_syscall = &leave_from_syscall_by_iret;
    }

 _rip = ip;
}

IMPLEMENT inline
Address
Return_frame::sp() const
{ return _rsp; }

IMPLEMENT inline
void
Return_frame::sp(Mword sp)
{ _rsp = sp; }

PUBLIC inline
Mword 
Return_frame::flags() const
{ return _rflags; }

PUBLIC inline
void
Return_frame::flags(Mword flags)
{ _rflags = flags; }

PUBLIC inline
Mword
Return_frame::cs() const
{ return _cs; }

PUBLIC inline
void
Return_frame::cs(Mword cs)
{ _cs = cs; }

PUBLIC inline
Mword
Return_frame::ss() const
{ return _ss; }

PUBLIC inline
void
Return_frame::ss(Mword ss)
{ _ss = ss; }

//---------------------------------------------------------------------------
IMPLEMENTATION [ux,amd64]:

//---------------------------------------------------------------------------
// IPC frame methods for IA32
// 
IMPLEMENT inline
Mword Syscall_frame::from_spec() const
{ return _rsi; }

IMPLEMENT inline
void Syscall_frame::from(Mword f)
{ _rsi = f; }

IMPLEMENT inline
L4_obj_ref Syscall_frame::ref() const
{ return L4_obj_ref::from_raw(_rdx); }

IMPLEMENT inline
void Syscall_frame::ref(L4_obj_ref const &r)
{ _rdx = r.raw(); }

IMPLEMENT inline
L4_timeout_pair Syscall_frame::timeout() const
{ return L4_timeout_pair(_rcx); }

IMPLEMENT inline 
void Syscall_frame::timeout(L4_timeout_pair const &to)
{ _rcx = to.raw(); }

IMPLEMENT inline Utcb *Syscall_frame::utcb() const
{ return reinterpret_cast<Utcb*>(_rdi); }

IMPLEMENT inline L4_msg_tag Syscall_frame::tag() const
{ return L4_msg_tag(_rax); }

IMPLEMENT inline
void Syscall_frame::tag(L4_msg_tag const &tag)
{ _rax = tag.raw(); }

