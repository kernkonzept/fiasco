INTERFACE [ppc32]:

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
  Unsigned8     _trapno;
  Unsigned16    _error;
  Mword         _cpsr, _sp;
public:
  void print(String_buffer *buf) const;
};

// --------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

PUBLIC inline
void
Tb_entry::rdtsc()
{}

// ------------------
PUBLIC inline
Unsigned16
Tb_entry_trap::cs() const
{ return 0; }

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
Tb_entry_trap::set(Mword pc, Trap_state *)
{
  _ip = pc;
  // More...
}

PUBLIC inline
void
Tb_entry_trap::set(Mword pc, Mword )
{
  _ip = pc;
}

