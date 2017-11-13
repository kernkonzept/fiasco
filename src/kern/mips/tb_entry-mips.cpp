/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Sanjay Lal <sanjayl@kymasys.com>
 * Alexander Warg <alexander.warg@kernkonzept.com>
 */

INTERFACE [mips]:

EXTENSION class Tb_entry
{
public:
  enum
  {
    Tb_entry_size = sizeof(Mword) * 16,
  };
};

/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  Unsigned32 _cause;
  Unsigned32 _status;
  Mword _sp;
public:
  void print(String_buffer *buf) const;
};

// --------------------------------------------------------------------
IMPLEMENTATION [mips]:

PUBLIC inline
void
Tb_entry::rdtsc()
{}

// ------------------
PUBLIC inline
Unsigned16
Tb_entry_trap::cs() const
{ return 0; }

PUBLIC inline NEEDS ["trap_state.h"]
Unsigned8
Tb_entry_trap::trapno() const
{ return Trap_state::Cause(_cause).exc_code(); }

PUBLIC inline
Unsigned32
Tb_entry_trap::error() const
{ return _cause; }

PUBLIC inline
Mword
Tb_entry_trap::sp() const
{ return _sp; }

PUBLIC inline
Mword
Tb_entry_trap::cr2() const
{ return 0; }

PUBLIC inline
Mword
Tb_entry_trap::eax() const
{ return 0; }

PUBLIC inline NEEDS ["trap_state.h"]
void
Tb_entry_trap::set(Mword pc, Trap_state *ts)
{
  _ip = pc;
  _cause = ts->cause;
  _status  = ts->status;
}

PUBLIC inline
void
Tb_entry_trap::set(Mword pc, Mword cause)
{
  _ip = pc;
  _cause = cause;
}

