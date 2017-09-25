INTERFACE [ia32,ux]:

EXTENSION class Tb_entry
{
public:
  enum
  {
    Tb_entry_size = 64,
  };
};

/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  Unsigned8	_trapno;
  Unsigned16	_error;
  Mword	_ebp, _cr2, _eax, _eflags, _esp;
  Unsigned16	_cs,  _ds;
public:
  void print(String_buffer *buf) const;
} __attribute__((packed));

IMPLEMENTATION [ia32,ux]:

#include "cpu.h"

PUBLIC inline NEEDS ["cpu.h"]
void
Tb_entry::rdtsc()
{ _tsc = Cpu::rdtsc(); }

PUBLIC inline NEEDS ["trap_state.h"]
void
Tb_entry_trap::set(Mword eip, Trap_state *ts)
{
  _ip     = eip;
  _trapno = ts->_trapno;
  _error  = ts->_err;
  _cr2    = ts->_cr2;
  _eax    = ts->_ax;
  _cs     = (Unsigned16)ts->cs();
  _ds     = (Unsigned16)ts->_ds;
  _esp    = ts->sp();
  _eflags = ts->flags();
}

PUBLIC inline
void
Tb_entry_trap::set(Mword eip, Mword trapno)
{
  _ip     = eip;
  _trapno = trapno;
  _cs     = 0;
}

PUBLIC inline
Unsigned8
Tb_entry_trap::trapno() const
{ return _trapno; }

PUBLIC inline
Unsigned16
Tb_entry_trap::error() const
{ return _error; }

PUBLIC inline
Mword
Tb_entry_trap::eax() const
{ return _eax; }

PUBLIC inline
Mword
Tb_entry_trap::cr2() const
{ return _cr2; }

PUBLIC inline
Mword
Tb_entry_trap::ebp() const
{ return _ebp; }

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
{ return _esp; }

PUBLIC inline
Mword
Tb_entry_trap::flags() const
{ return _eflags; }
