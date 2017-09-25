INTERFACE[sparc]:

#include "types.h"

EXTENSION class Syscall_frame
{
  public:
    Mword r[30]; //{r0, r2, r3, ..., r10, r13 .., r31, ip
    void dump() const;
};

EXTENSION class Return_frame
{
  public:
    Mword xer;  //+32
    Mword ctr;  //+28
    Mword cr;   //+24
    Mword srr1; //+20
    Mword srr0; //+16
    Mword ulr;  //+12
    Mword usp;  //+8
    Mword r11;  //+4 --two scratch registers for exception entry
    Mword r12;  //0
    void dump();
    void dump_scratch();
    bool user_mode();
};

//------------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include "warn.h"
#include "psr.h"

IMPLEMENT
void
Syscall_frame::dump() const
{
  printf("IP: %08lx\n", r[29]);
  printf("R[%2d]: %08lx R[%2d]: %08lx R[%2d]: %08lx R[%2d]: %08lx R[%2d]: %08lx\n",
          0, r[0], 2, r[1], 3, r[2], 4, r[3], 5, r[4]);
  printf("R[%2d]: %08lx R[%2d]: %08lx R[%2d]: %08lx R[%2d]: %08lx R[%2d]: %08lx\n",
          6, r[5], 7, r[6], 8, r[7], 9, r[8], 10, r[9]);
}

PRIVATE
void
Return_frame::srr1_bit_scan()
{
  printf("SRR1 bits:");
  for(int i = 31; i >= 0; i--)
    if(srr1 & (1 << i))
     printf(" %d", 31-i);
  printf("\n");
}

IMPLEMENT
void
Return_frame::dump()
{
  printf("SRR0 %08lx SRR1 %08lx SP %08lx\n"
         "LR   %08lx CTR  %08lx CR %08lx XER %08lx\n",
         srr0, srr1, usp, ulr, ctr, cr, xer);
  srr1_bit_scan();
}

IMPLEMENT
void
Return_frame::dump_scratch()
{
  printf("\nR[%2d]: %08lx\nR[%2d]: %08lx\n", 11, r11, 12, r12);
}

IMPLEMENT inline
Mword
Return_frame::sp() const
{
  return Return_frame::usp;
}

IMPLEMENT inline
void
Return_frame::sp(Mword _sp)
{
  Return_frame::usp = _sp;
}

IMPLEMENT inline
Mword
Return_frame::ip() const
{
  return Return_frame::srr0;
}

IMPLEMENT inline
void
Return_frame::ip(Mword _pc)
{
  Return_frame::srr0 = _pc;
}

IMPLEMENT inline NEEDS ["psr.h"]
bool
Return_frame::user_mode()
{
  return 0;
  /*return Msr::Msr_pr & srr1;*/
}

//---------------------------------------------------------------------------
//TODO cbass: set registers properly 
IMPLEMENT inline
void Syscall_frame::from(Mword id)
{ r[5] = id; /*r6*/ }

IMPLEMENT inline
Mword Syscall_frame::from_spec() const
{ return r[5]; /*r6*/ }


IMPLEMENT inline
L4_obj_ref Syscall_frame::ref() const
{ return L4_obj_ref::from_raw(r[3]); /*r4*/ }

IMPLEMENT inline
void Syscall_frame::ref(L4_obj_ref const &ref)
{ r[3] = ref.raw(); /*r4*/ }

IMPLEMENT inline
L4_timeout_pair Syscall_frame::timeout() const
{ return L4_timeout_pair(r[4]); /*r5*/ }

IMPLEMENT inline 
void Syscall_frame::timeout(L4_timeout_pair const &to)
{ r[4] = to.raw(); /*r5*/ }

IMPLEMENT inline Utcb *Syscall_frame::utcb() const
{ return reinterpret_cast<Utcb*>(r[1]); /*r2*/}

IMPLEMENT inline L4_msg_tag Syscall_frame::tag() const
{ return L4_msg_tag(r[2]); /*r3*/ }

IMPLEMENT inline
void Syscall_frame::tag(L4_msg_tag const &tag)
{ r[2] = tag.raw(); /*r3*/ }

IMPLEMENT inline
Mword
Return_frame::ip_syscall_page_user() const
{ return Return_frame::srr0; }
