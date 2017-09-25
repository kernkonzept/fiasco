INTERFACE [amd64]:

#include "types.h"

class Syscall_entry
{
private:
  template<int INSN_LEN>
  struct Mem_insn
  {
    Unsigned32 _insn:INSN_LEN * 8;
    Unsigned32 _offset;
    Mem_insn(Unsigned32 insn, void *mem)
    : _insn(insn),
      _offset((Address)mem - (Address)(&_offset + 1))
    {}
  } __attribute__((packed));

  Mem_insn<3> _mov_rsp_user_sp;
  Mem_insn<3> _mov_kern_sp_rsp;
  Unsigned32 _mov_rsp_rsp;
  Unsigned8 _push_ss, _ss_value;
  Mem_insn<2> _push_user_rsp;
  Unsigned8 _jmp;
  Signed32 _entry_offset;
  Unsigned8 _pading[33]; // pad to the next 64 byte boundary
  Unsigned64 _kern_sp;
  Unsigned64 _user_sp;
} __attribute__((packed, aligned(64)));


IMPLEMENTATION [amd64]:

#include "config_gdt.h"

PUBLIC inline NEEDS["config_gdt.h"]
Syscall_entry::Syscall_entry()
: /* mov %rsp, _user_sp(%rip) */ _mov_rsp_user_sp(0x258948, &_user_sp),
  /* mov _kern_sp(%rip), %rsp */ _mov_kern_sp_rsp(0x258b48, &_kern_sp),
  /* mov (%rsp), %rsp */         _mov_rsp_rsp(0x24248b48),
  /* pushq GDT_DATA_USER | 3 */  _push_ss(0x6a), _ss_value(GDT_DATA_USER | 3),
  /* pushq _user_sp(%rip) */     _push_user_rsp(0x35ff, &_user_sp),
  /* jmp *_entry_offset */       _jmp(0xe9)
{}

PUBLIC inline
void
Syscall_entry::set_entry(void (*func)(void))
{
  _entry_offset = (Address)func
                  - ((Address)&_entry_offset + sizeof(_entry_offset));
}

PUBLIC inline
void
Syscall_entry::set_rsp(Address rsp)
{
  _kern_sp = rsp;
}


