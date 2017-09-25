INTERFACE [arm]:

EXTENSION class Tb_entry
{
public:
  enum
  {
    Tb_entry_size = 64,
  };
  static Unsigned64 (*read_cycle_counter)();
};

/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  Unsigned32    _error;
  Mword         _cpsr, _sp;
public:
  void print(String_buffer *buf) const;
};

// --------------------------------------------------------------------
IMPLEMENTATION [arm]:

PROTECTED static Unsigned64 Tb_entry::dummy_read_cycle_counter() { return 0; }

Unsigned64 (*Tb_entry::read_cycle_counter)() = dummy_read_cycle_counter;

PUBLIC static
void
Tb_entry::set_cycle_read_func(Unsigned64 (*f)())
{ read_cycle_counter = f; }

PUBLIC inline
void
Tb_entry::rdtsc()
{ _tsc = read_cycle_counter(); }

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
Tb_entry_trap::set(Mword ip, Trap_state *ts)
{
  _ip    = ip;
  _error = ts->error_code;
  _cpsr  = ts->psr;
  _sp    = ts->sp();
}

PUBLIC inline NEEDS ["trap_state.h"]
void
Tb_entry_trap::set(Mword pc, Mword)
{
  _ip    = pc;
}
