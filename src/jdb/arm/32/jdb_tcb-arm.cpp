IMPLEMENTATION [arm]:

#include "config.h"

EXTENSION class Jdb_tcb
{
  enum
  {
    Disasm_x = 41,
    Disasm_y = 11,
    Stack_y  = 17,
  };

};

IMPLEMENT
void Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(Jdb::current_cpu);
  int from_user       = ef->from_user();

  printf("Registers (before debug entry from %s mode):\n"
         "[0] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "[8] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %s%08lx\033[m\n"
         "upsr=%08lx tpidr: urw=%08lx uro=%08lx\n",
         from_user ? "user" : "kernel",
	 ef->r[0], ef->r[1],ef->r[2], ef->r[3],
	 ef->r[4], ef->r[5],ef->r[6], ef->r[7],
	 ef->r[8], ef->r[9],ef->r[10], ef->r[11],
	 ef->r[12], ef->usp, ef->ulr, Jdb::esc_iret, ef->pc,
	 ef->psr, t->tpidrurw(), t->tpidruro());
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("PC=%s%08lx\033[m USP=%08lx\n",
         Jdb::esc_emph, current.top_value(-2), current.top_value(-5));
  printf("[0] %08lx %08lx %08lx %08lx [4] %08lx %08lx %08lx %08lx\n",
         current.top_value(-18), current.top_value(-17),
         current.top_value(-16), current.top_value(-15),
         current.top_value(-14), current.top_value(-13),
         current.top_value(-12), current.top_value(-11));
  printf("[8] %08lx %08lx %08lx %08lx [c] %08lx %08lx %08lx %08lx\n",
         current.top_value(-10), current.top_value(-9),
         current.top_value(-8),  current.top_value(-7),
         current.top_value(-6),  current.top_value(-4),
         current.top_value(-3),  current.top_value(-2));
}

IMPLEMENT
void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &, Address)
{}

IMPLEMENT_OVERRIDE
bool
Jdb_stack_view::edit_registers()
{ return false; }

IMPLEMENT inline
bool
Jdb_tcb_ptr::is_user_value() const
{
  return _offs >= Context::Size - 5 * sizeof(Mword);
}

IMPLEMENT inline
const char *
Jdb_tcb_ptr::user_value_desc() const
{
  const char *desc[] = { "PSR", "PC", "KLR", "ULR", "SP" };
  return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
}
