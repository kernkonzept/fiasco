/*
 * Fiasco Kernel-Entry Frame-Layout Code
 * Shared between UX and native IA32.
 */

INTERFACE[ia32,ux]:

#include "types.h"

EXTENSION class Syscall_frame
{
protected:
  Mword             _ecx;
  Mword             _edx;
  Mword             _esi;
  Mword             _edi;
  Mword             _ebx;
  Mword             _ebp;
  Mword             _eax;
};

EXTENSION class Return_frame
{
private:
  Mword             _eip;
  Unsigned16  _cs, __attribute__((unused)) __csu;
  Mword          _eflags;
  Mword             _esp;
  Unsigned16  _ss, __attribute__((unused)) __ssu;

public:
  enum { Pf_ax_offset = 0 };
};

IMPLEMENTATION[ia32,ux]:

//---------------------------------------------------------------------------
// basic Entry_frame methods for IA32
// 
#include "mem_layout.h"

IMPLEMENT inline
Address
Return_frame::ip() const
{ return _eip; }

IMPLEMENT inline NEEDS["mem_layout.h"]
Address
Return_frame::ip_syscall_page_user() const
{
  Address eip = ip();
  if ((eip & Mem_layout::Syscalls) == Mem_layout::Syscalls)
     eip = *(Mword*)sp();
  return eip;
}


IMPLEMENT inline
Address
Return_frame::sp() const
{ return _esp; }

IMPLEMENT inline
void
Return_frame::sp(Mword sp)
{ _esp = sp; }

PUBLIC inline
Mword 
Return_frame::flags() const
{ return _eflags; }

PUBLIC inline
void
Return_frame::flags(Mword flags)
{ _eflags = flags; }

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
IMPLEMENTATION [ia32,ux]:

//---------------------------------------------------------------------------
// IPC frame methods for IA32
// 
IMPLEMENT inline
Mword Syscall_frame::from_spec() const
{ return _esi; }

IMPLEMENT inline
void Syscall_frame::from(Mword f)
{ _esi = f; }

IMPLEMENT inline
L4_obj_ref Syscall_frame::ref() const
{ return L4_obj_ref::from_raw(_edx); }

IMPLEMENT inline
void Syscall_frame::ref(L4_obj_ref const &ref)
{ _edx = ref.raw(); }

IMPLEMENT inline
L4_timeout_pair Syscall_frame::timeout() const
{ return L4_timeout_pair(_ecx); }

IMPLEMENT inline 
void Syscall_frame::timeout(L4_timeout_pair const &to)
{ _ecx = to.raw(); }

IMPLEMENT inline Utcb *Syscall_frame::utcb() const
{ return reinterpret_cast<Utcb*>(_edi); }

IMPLEMENT inline L4_msg_tag Syscall_frame::tag() const
{ return L4_msg_tag(_eax); }

IMPLEMENT inline
void Syscall_frame::tag(L4_msg_tag const &tag)
{ _eax = tag.raw(); }


// ---------------------------------------------------------------------------
IMPLEMENTATION [ia32]:

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
      /* symbols from the assembler entry code */
      extern Mword leave_from_sysenter_by_iret;
      extern Mword leave_alien_from_sysenter_by_iret;
      extern Mword ret_from_fast_alien_ipc;
      Mword **ret_from_disp_syscall = reinterpret_cast<Mword**>(static_cast<Entry_frame*>(this))-1;
      cs(cs() & ~0x80);
      if (*ret_from_disp_syscall == &ret_from_fast_alien_ipc)
	*ret_from_disp_syscall = &leave_alien_from_sysenter_by_iret;
      else
	*ret_from_disp_syscall = &leave_from_sysenter_by_iret;
    }

  _eip = ip;
}


// ---------------------------------------------------------------------------
IMPLEMENTATION [ux]:

IMPLEMENT inline
void
Return_frame::ip(Mword ip)
{
  _eip = ip;
}
