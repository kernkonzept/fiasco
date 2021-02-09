IMPLEMENTATION [arm]:

#include "config.h"

EXTENSION class Jdb_tcb
{
  enum
  {
    Disasm_x = 50,
    Disasm_y = 11,
    Stack_y  = 17,
  };

};

PRIVATE static
void
Jdb_tcb::print_gp_regs(Mword const *r)
{
  printf(" r0 %08lx %08lx %08lx %08lx\n r4 %08lx %08lx %08lx %08lx\n"
         " r8 %08lx %08lx %08lx %08lx\nr12 %08lx ",
         r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11], r[12]);
}

IMPLEMENT
void
Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(Jdb::current_cpu);

  printf("Regs (before debug entry from %s mode):\n",
         ef->from_user() ? "user" : "kernel");

  print_gp_regs(&ef->r[0]);

  printf("%08lx %08lx %s%08lx\033[m\n"
         "psr=%08lx tpidr: urw=%08lx uro=%08lx\n",
         ef->usp, ef->ulr, Jdb::esc_iret, ef->pc,
         ef->psr, t->tpidrurw(), t->tpidruro());
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("PC=%s%08lx\033[m USP=%08lx\n",
         Jdb::esc_emph, current.top_value(-2), current.top_value(-5));

  print_gp_regs(current.top_value_ptr(-18));

  printf("%08lx %08lx %08lx\n",
         current.top_value(-4), current.top_value(-3),  current.top_value(-2));
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

IMPLEMENT_OVERRIDE
Address
Jdb_tcb_ptr::user_ip() const
{
  return top_value(-2);
}
