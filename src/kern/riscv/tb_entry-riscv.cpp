INTERFACE [riscv]:

EXTENSION class Tb_entry
{
public:
  enum
  {
    Tb_entry_size = 16 * sizeof(Mword),
  };
};

/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  Unsigned32 _cause;
  Mword _sp;
public:
  void print(String_buffer *buf) const;
};
static_assert(sizeof(Tb_entry_trap) <= Tb_entry::Tb_entry_size);

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

PUBLIC inline
void
Tb_entry::rdtsc()
{ _tsc = 0; }

// ------------------
PUBLIC inline
Unsigned16
Tb_entry_trap::cs() const
{ return 0; }

PUBLIC inline
Unsigned8
Tb_entry_trap::trapno() const
{ return 0; }

PUBLIC inline
Unsigned32
Tb_entry_trap::error() const
{  return _cause; }

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
  _sp = ts->sp();
}

PUBLIC inline
void
Tb_entry_trap::set(Mword pc, Mword cause)
{
  _ip = pc;
  _cause = cause;
  _sp = 0;
}
