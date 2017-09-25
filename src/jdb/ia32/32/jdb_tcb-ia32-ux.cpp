IMPLEMENTATION [ia32,ux]:

#include "config.h"
#include "jdb.h"
#include "jdb_disasm.h"

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
void
Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(Jdb::current_cpu);
  int from_user       = ef->from_user();
  Address disass_addr = ef->ip();

  // registers, disassemble
  printf("EAX=%08lx  ESI=%08lx  DS=%04lx\n"
	 "EBX=%08lx  EDI=%08lx  ES=%04lx     ",
	 ef->_ax, ef->_si, ef->_ds & 0xffff, ef->_bx, ef->_di, ef->_es & 0xffff);

  if (Jdb_disasm::avail())
    {
      putstr(Jdb::esc_emph);
      Jdb_disasm::show_disasm_line(-40, disass_addr, 0, from_user ? t->space() : 0);
      putstr("\033[m");
    }

  printf("ECX=%08lx  EBP=%08lx  GS=%04lx     ",
	 ef->_cx, ef->_bp, ef->_gs & 0xffff);

  if (Jdb_disasm::avail())
    Jdb_disasm::show_disasm_line(-40, disass_addr, 0, from_user ? t->space() : 0);

  printf("EDX=%08lx  ESP=%08lx  SS=%04lx\n"
	 "trap %lu (%s), error %08lx, from %s mode\n"
	 "CS=%04lx  EIP=%s%08lx\033[m  EFlags=%08lx\n",
	 ef->_dx, ef->sp(), ef->ss() & 0xffff,
	 ef->_trapno, Cpu::exception_string(ef->_trapno), ef->_err,
	 from_user ? "user" : "kernel",
	 ef->cs() & 0xffff, Jdb::esc_emph, ef->ip(), ef->flags());

  if (ef->_trapno == 14)
    printf("page fault linear address " L4_PTR_FMT "\n", ef->_cr2);
}

IMPLEMENT
void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &current, Mword ksp)
{
  printf("CS=%04lx  EIP=%s%08lx\033[m  EFlags=%08lx kernel ESP=%08lx",
      current.top_value(-4) & 0xffff, Jdb::esc_emph, 
      current.top_value(-5), current.top_value(-3), ksp);
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  Jdb::Guessed_thread_state state = Jdb::guess_thread_state(t);
  Jdb_tcb_ptr p((Address)t->get_kernel_sp());
  int sub = 0;

  switch (state)
    {
    case Jdb::s_ipc:
    case Jdb::s_syscall:
      printf("EAX=%08lx  ESI=%08lx\n"
	     "EBX=%08lx  EDI=%08lx\n"
    	     "ECX=%08lx  EBP=%08lx\n"
	     "EDX=%08lx  ESP=%08lx  SS=%04lx\n"
	     "in %s (user level registers)",
	     p.top_value( -6), p.top_value(-10),
	     p.top_value( -8), p.top_value( -9),
	     p.top_value(-12), p.top_value( -7),
	     p.top_value(-11), p.top_value( -2), p.top_value(-1) & 0xffff,
	     state == Jdb::s_ipc ? "ipc" : "syscall");
      break;
    case Jdb::s_user_invoke:
      printf("EAX=00000000  ESI=00000000\n"
	     "EBX=00000000  EDI=00000000\n"
	     "ECX=00000000  EBP=00000000\n"
	     "EDX=00000000  ESP=%08lx  SS=%04lx\n"
	     "invoking user the first time (user level registers)",
	     p.top_value(-2), p.top_value(-1) & 0xffff);
      break;
    case Jdb::s_fputrap:
      printf("EAX=%08lx\n"
	     "\n"
    	     "ECX=%08lx\n"
	     "EDX=%08lx  ESP=%08lx  SS=%04lx\n"
	     "in exception #0x07 (user level registers)",
	     p.top_value(-7), p.top_value(-8),
	     p.top_value(-9), p.top_value(-2), p.top_value(-1) & 0xffff);
      break;
    case Jdb::s_pagefault:
      printf("EAX=%08lx\n"
	     "\n"
    	     "ECX=%08lx  EBP=%08lx\n"
	     "EDX=%08lx  ESP=%08lx  SS=%04lx\n"
	     "in page fault, error %08lx (user level registers)\n"
	     "\n"
	     "page fault linear address %08lx",
	     p.top_value( -7), p.top_value( -8), p.top_value(-6), 
	     p.top_value( -9), p.top_value( -2), p.top_value(-1) & 0xffff, 
	     p.top_value(-11), p.top_value(-10));
      break;
    case Jdb::s_interrupt:
      printf("EAX=%08lx\n"
	     "\n"
    	     "ECX=%08lx\n"
	     "EDX=%08lx  ESP=%08lx  SS=%04lx\n"
	     "in interrupt #0x%02lx (user level registers)",
	     p.top_value(-6), p.top_value(-8),
	     p.top_value(-7), p.top_value(-2), p.top_value(-1) & 0xffff,
	     p.top_value(-9));
      break;
    case Jdb::s_timer_interrupt:
      if (Config::Have_frame_ptr)
	sub = -1;
      printf("EAX=%08lx\n"
	     "\n"
    	     "ECX=%08lx\n"
	     "EDX=%08lx  ESP=%08lx  SS=%04lx\n"
	     "in timer interrupt (user level registers)",
	     p.top_value(-6-sub), p.top_value(-8-sub),
	     p.top_value(-7-sub), p.top_value(-2), p.top_value(-1) & 0xffff);
      break;
    case Jdb::s_slowtrap:
      printf("EAX=%08lx  ESI=%08lx  DS=%04lx\n"
             "EBX=%08lx  EDI=%08lx  ES=%04lx\n"
             "ECX=%08lx  EBP=%08lx  GS=%04lx\n"
             "EDX=%08lx  ESP=%08lx  SS=%04lx\n"
             "in exception %lu, error %08lx (user level registers)",
	     p.top_value( -8), p.top_value(-14), p.top_value(-18) & 0xffff, 
	     p.top_value(-11), p.top_value(-15), p.top_value(-19) & 0xffff,
	     p.top_value( -9), p.top_value(-13), p.top_value(-17) & 0xffff,
	     p.top_value(-10), p.top_value( -2), p.top_value( -1) & 0xffff,
	     p.top_value( -7), p.top_value( -6));
      break;
    case Jdb::s_unknown:
      break;
    }
}

IMPLEMENT_OVERRIDE
bool
Jdb_stack_view::edit_registers()
{
  Mword value;
  char reg = (char)(-1);
  Mword *reg_ptr=0;
  unsigned x=0, y=0;


  Jdb::printf_statline("tcb", 0, "edit register "
      "e{ax|bx|cx|dx|si|di|sp|bp|ip|fl}: ");
  Jdb::cursor(Jdb_screen::height(), 53);
  Jdb::get_register(&reg);

  switch (reg)
    {
    case  1: x= 4; y= 9; reg_ptr = &ef->_ax; break;
    case  2: x= 4; y=10; reg_ptr = &ef->_bx; break;
    case  3: x= 4; y=11; reg_ptr = &ef->_cx; break;
    case  4: x= 4; y=12; reg_ptr = &ef->_dx; break;
    case  5: x=18; y=11; reg_ptr = &ef->_bp; break;
    case  6: x=18; y= 9; reg_ptr = &ef->_si; break;
    case  7: x=18; y=10; reg_ptr = &ef->_di; break;
    case  8: x=13; y=14; reg_ptr = &ef->_ip; break;
    case  9: // we have no esp if we come from kernel
             if (!ef->from_user())
               return false;
             x=18; y=12; reg_ptr = &ef->_sp; break;
    case 10: x=35; y=12; reg_ptr = &ef->_flags; 
             break;
    default: return false;
    }

  Jdb::cursor(y+1, x+1);
  putstr("        ");
  Jdb::printf_statline("tcb", 0, "edit %s = %08lx", 
      Jdb_screen::Reg_names[reg-1], *reg_ptr);
  Jdb::cursor(y+1, x+1);
  if (Jdb_input::get_mword(&value, 8, 16))
    {
    *reg_ptr = value;
    Jdb::cursor(y+1, x+1);
    printf("%08lx", *reg_ptr);
    }
  return true;
}

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
  const char *desc[] = { "SS", "SP", "EFL", "CS", "IP" };
  return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
}
