IMPLEMENTATION [riscv]:

#include "config.h"
#include "entry_frame.h"

EXTENSION class Jdb_tcb
{
  enum
  {
    Disasm_x = 48,
    Disasm_y = 11,
    Stack_y  = 17,
  };
};

PRIVATE static inline
bool
Jdb_tcb::is_extended()
{
  return Jdb_screen::height() > 27;
}

IMPLEMENT_OVERRIDE
unsigned
Jdb_tcb::stack_y()
{
  return Stack_y + (is_extended() ? 10 : 0);
}

PRIVATE static
void
Jdb_tcb::print_entry_frame(Entry_frame *ef)
{
  printf("Regs (before debug entry from %s mode):\n",
         ef->user_mode() ? "user" : "kernel");
  ef->dump(is_extended());
}

IMPLEMENT
void
Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(t->get_current_cpu());
  print_entry_frame(ef);
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  print_entry_frame(t->regs());
}

IMPLEMENT
void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &, Address)
{}

IMPLEMENT_OVERRIDE
bool
Jdb_stack_view::edit_registers()
{ return false; }

EXTENSION class Jdb_tcb_ptr
{
  // Entry frame might be padded from the bottom of the stack to align it with
  // stack alignment required by architecture.
  static constexpr unsigned Frame_pad_size =
    Cpu::stack_round(sizeof(Trap_state)) - sizeof(Trap_state);
};

IMPLEMENT inline
bool
Jdb_tcb_ptr::is_user_value() const
{
  return    _offs >= Context::Size - sizeof(Trap_state)
         && _offs < Context::Size - Frame_pad_size;
}

IMPLEMENT inline
const char *
Jdb_tcb_ptr::user_value_desc() const
{
  const char *const desc[] =
    {
      "hstatus", "tval", "cause", "status", "pc",
      "t6", "t5", "t4", "t3",
      "s11", "s10", "s9", "s8", "s7", "s6", "s5", "s4", "s3", "s2",
      "a7", "a6", "a5", "a4", "a3", "a2", "a1", "a0",
      "s1", "s0",
      "t2", "t1", "t0",
      "tp", "gp", "sp", "ra",
      "eret_work"
    };
  static_assert(
    (sizeof(Trap_state) / sizeof(Mword)) == (sizeof(desc) / sizeof(desc[0])),
    "desc entries do not match the sizeof Trap_state");
  return desc[(Context::Size - Frame_pad_size - _offs) / sizeof(Mword) - 1];
}

IMPLEMENT_OVERRIDE
Address
Jdb_tcb_ptr::user_ip() const
{
  unsigned pc_offs = sizeof(Trap_state) - offsetof(Trap_state, _pc);
  return top_value(-(Frame_pad_size + pc_offs) / sizeof(Mword));
}
