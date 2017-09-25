INTERFACE [sparc]:

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
  struct Payload
  {
    Unsigned8     _trapno;
    Unsigned16    _error;
    Mword         _cpsr, _sp;
  };
public:
  void print(String_buffer *buf) const;
};

// --------------------------------------------------------------------
IMPLEMENTATION [sparc]:

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
{ return 0; }

PUBLIC inline
Unsigned16
Tb_entry_trap::error() const
{ return 0; }

PUBLIC inline
Mword
Tb_entry_trap::sp() const
{ return 0; }

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
Tb_entry_trap::set(Context const *ctx, Mword pc, Trap_state *)
{
  (void)pc;
  (void)ctx;
}

PUBLIC inline NEEDS ["trap_state.h"]
void
Tb_entry_trap::set(Context const *ctx, Mword pc, Mword)
{
  (void)pc;
  (void)ctx;
}
