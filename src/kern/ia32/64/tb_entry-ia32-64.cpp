INTERFACE [amd64]:

EXTENSION class Tb_entry
{
public:
  enum
  {
    Tb_entry_size = 128,
  };
};

/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  char	_trapno;
  Unsigned16	_error;
  Mword	_rbp, _cr2, _rax, _rflags, _rsp;
  Unsigned16	_cs,  _ds;
public:
  void print(String_buffer *buf) const;
} __attribute__((packed));

IMPLEMENTATION [amd64]:

#include "cpu.h"

PUBLIC inline NEEDS ["cpu.h"]
void
Tb_entry::rdtsc()
{ _tsc = Cpu::rdtsc(); }

PUBLIC inline NEEDS ["trap_state.h"]
void
Tb_entry_trap::set(Mword rip, Trap_state *ts)
{
  _ip = rip;
  _trapno = ts->_trapno;
  _error  = ts->_err;
  _cr2    = ts->_cr2;
  _rax    = ts->_ax;
  _cs     = (Unsigned16)ts->cs();
  _rsp    = ts->sp();
  _rflags = ts->flags();
}

PUBLIC inline
void
Tb_entry_trap::set(Mword eip, Mword trapno)
{
  _ip = eip;
  _trapno = trapno | 0x80;
}

PUBLIC inline
char
Tb_entry_trap::trapno() const
{ return _trapno; }

PUBLIC inline
Unsigned16
Tb_entry_trap::error() const
{ return _error; }

PUBLIC inline
Mword
Tb_entry_trap::eax() const
{ return _rax; }

PUBLIC inline
Mword
Tb_entry_trap::cr2() const
{ return _cr2; }

PUBLIC inline
Mword
Tb_entry_trap::ebp() const
{ return _rbp; }

PUBLIC inline
Unsigned16
Tb_entry_trap::cs() const
{ return _cs; }

PUBLIC inline
Unsigned16
Tb_entry_trap::ds() const
{ return _ds; }

PUBLIC inline
Mword
Tb_entry_trap::sp() const
{ return _rsp; }

PUBLIC inline
Mword
Tb_entry_trap::flags() const
{ return _rflags; }


