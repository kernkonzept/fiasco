IMPLEMENTATION [arm]:

#include "config.h"

EXTENSION class Jdb_tcb
{
  enum
  {
    Disasm_x = 48,
    Disasm_y = 11,
    Stack_y  = 18,
  };

};

IMPLEMENT
void Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(Jdb::current_cpu);
  int from_user       = ef->from_user();

  printf("Regs (before debug entry from %s mode):\n"
         "x0=%016lx   x1=%016lx\n"
         "x2=%016lx   x3=%016lx\n"
         "x4=%016lx   x5=%016lx   x6=%016lx   x7=%016lx\n"
         "x8=%016lx   x9=%016lx  x10=%016lx  x11=%016lx\n"
         "upsr=%016lx tpidr: urw=%016lx uro=%016lx\n",
         from_user ? "user" : "kernel",
         ef->r[0], ef->r[1], ef->r[2], ef->r[3],
         ef->r[4], ef->r[5], ef->r[6], ef->r[7],
         ef->r[8], ef->r[9], ef->r[10], ef->r[11],
         ef->psr, t->tpidrurw(), t->tpidruro());
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("PC=%s%016lx\033[m USP=%016lx\n"
         "  [0] %016lx %016lx\n"
         "  [2] %016lx %016lx\n",
         Jdb::esc_emph,
         current.top_value(-2),  current.top_value(-5),
         current.top_value(-18), current.top_value(-17),
         current.top_value(-16), current.top_value(-15));

  unsigned cols = Jdb_screen::cols(5, 17) - 1;
  if (cols > 6)
    cols = 6;
  for (unsigned i = 0, j = 0; i < 12; ++i)
    {
      if ((i % cols) == 0)
        {
          if (++j > 6)
            break;
          printf("  [%x] ", i+4);
        }

      printf("%016lx%s", current.top_value(-14 + i),
             ((i % cols) == (cols-1)) ? "\n" : " ");
    }
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
  return _offs >= Context::Size - 6 * sizeof(Mword);
}

IMPLEMENT inline
const char *
Jdb_tcb_ptr::user_value_desc() const
{
  const char *desc[] = { "PSR", "PC", "USP", "PFA", "ESR", "KSP" };
  return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
}

IMPLEMENT_OVERRIDE
Address
Jdb_tcb_ptr::user_ip() const
{
  return top_value(-2);
}
