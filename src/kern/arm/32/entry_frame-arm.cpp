/*
 * Fiasco Kernel-Entry Frame-Layout Code for ARM
 */
INTERFACE [arm]:

#include "types.h"

EXTENSION class Syscall_frame
{
public:
  //protected:
  Mword r[13];
};

EXTENSION class Return_frame
{
public:
  //protected:
  Mword usp;
  Mword ulr;
  Mword km_lr;
  Mword pc;
  Mword psr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstdio>
#include "processor.h"

PUBLIC inline NEEDS["processor.h"]
void
Return_frame::psr_set_mode(unsigned char m)
{
  psr = (psr & ~Proc::Status_mode_mask) | m;
}

IMPLEMENT inline
Mword
Return_frame::ip() const
{ return Return_frame::pc; }

IMPLEMENT inline
Mword
Return_frame::ip_syscall_user() const
{ return Return_frame::pc; }

IMPLEMENT inline
void
Return_frame::ip(Mword _pc)
{ Return_frame::pc = _pc; }

IMPLEMENT inline
Mword
Return_frame::sp() const
{ return Return_frame::usp; }

IMPLEMENT inline
void
Return_frame::sp(Mword sp)
{ Return_frame::usp = sp; }

//---------------------------------------------------------------------------
IMPLEMENT inline
void Syscall_frame::from(Mword id)
{ r[4] = id; }

IMPLEMENT inline
Mword Syscall_frame::from_spec() const
{ return r[4]; }


IMPLEMENT inline
L4_obj_ref Syscall_frame::ref() const
{ return L4_obj_ref::from_raw(r[2]); }

IMPLEMENT inline
void Syscall_frame::ref(L4_obj_ref const &ref)
{ r[2] = ref.raw(); }

IMPLEMENT inline
L4_timeout_pair Syscall_frame::timeout() const
{ return L4_timeout_pair(r[3]); }

IMPLEMENT inline 
void Syscall_frame::timeout(L4_timeout_pair const &to)
{ r[3] = to.raw(); }

IMPLEMENT inline Utcb *Syscall_frame::utcb() const
{ return reinterpret_cast<Utcb*>(r[1]); }

IMPLEMENT inline L4_msg_tag Syscall_frame::tag() const
{ return L4_msg_tag(r[0]); }

IMPLEMENT inline
void Syscall_frame::tag(L4_msg_tag const &tag)
{ r[0] = tag.raw(); }

//------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

PUBLIC inline
bool
Return_frame::check_valid_user_psr() const
{ return (psr & Proc::Status_mode_mask) == Proc::PSR_m_usr; }

//------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

PUBLIC inline
bool
Return_frame::check_valid_user_psr() const
{ return (psr & Proc::Status_mode_mask) != Proc::PSR_m_hyp; }

