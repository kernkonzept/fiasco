IMPLEMENTATION [amd64]:

#include "jdb_disasm.h"

EXTENSION class Jdb_tcb
{
  enum
  {
    Disasm_x = 45,
    Disasm_y = 11,
    Stack_y  = 21,
  };
};

IMPLEMENT
void
Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(t->get_current_cpu());
  char pfa[32] = "";

  if (ef->_trapno == 14)
    snprintf(pfa, sizeof(pfa), "(PFA " L4_PTR_FMT ")", ef->_cr2);

  // registers, disassemble
  printf("RAX=%016lx  RSI=%016lx\n"
         "RBX=%016lx  RDI=%016lx\n"
         "RCX=%016lx  RBP=%016lx\n"
         "RDX=%016lx  RSP=%016lx\n"
         " R8=%016lx   R9=%016lx\n"
         "R10=%016lx  R11=%016lx\n"
         "R12=%016lx  R13=%016lx\n"
         "R14=%016lx  R15=%016lx  FS=%04lx\n"
	 "trapno %lu, error %08lx, from %s mode%s\n"
	 "RIP=%s%016lx\033[m  RFlags=%016lx\n",
	 ef->_ax, ef->_si, ef->_bx, ef->_di,
	 ef->_cx, ef->_bp, ef->_dx, ef->sp(),
	 ef->_r8,  ef->_r9, ef->_r10, ef->_r11,
	 ef->_r12, ef->_r13, ef->_r14, ef->_r15, t->_fs_base,
	 ef->_trapno, ef->_err, ef->from_user() ? "user" : "kernel",
         ef->_trapno == 14 ? pfa : "",
	 Jdb::esc_emph, ef->ip(), ef->flags());
}

IMPLEMENT
void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &current, Mword ksp)
{
  printf("RIP=%s%016lx\033[m  RFlags=%016lx kernel RSP=%016lx\n",
         Jdb::esc_emph, current.top_value(-5), current.top_value(-3), ksp);
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  Jdb::Guessed_thread_state state = Jdb::guess_thread_state(t);
  Jdb_tcb_ptr p(reinterpret_cast<Address>(t->get_kernel_sp()));
  int sub = 0;

  switch (state)
    {
    case Jdb::s_ipc:
      printf("RAX=%016lx  RSI=%016lx\n"
	     "RBX=%016lx  RDI=%016lx\n"
             "RCX=%016lx  RBP=%016lx\n"
             "RDX=%016lx  RSP=%016lx\n"
             "R8= %016lx  R9= %016lx\n"
             "R10=%016lx  R11=%016lx\n"
             "R12=%016lx  R13=%016lx  SS=%04lx\n"
             "R14=%016lx  R15=%016lx  CS=%04lx\n"
	     "in %s (user level registers)\n",
	     p.top_value( -6), p.top_value(-10),
	     p.top_value( -8), p.top_value( -9),
	     p.top_value(-12), p.top_value( -7),
	     p.top_value(-11), p.top_value( -2),
	     p.top_value(-19), p.top_value(-18),
	     p.top_value(-17), p.top_value(-16),
	     p.top_value(-15), p.top_value(-14), p.top_value(-1) & 0xffff,
	     p.top_value(-13), p.top_value(-12), p.top_value(-4) & 0xffff,
	     state == Jdb::s_ipc ? "ipc" : "syscall");
      break;
    case Jdb::s_user_invoke:
      printf("RAX=0000000000000000  RSI=0000000000000000\n"
	     "RBX=0000000000000000  RDI=0000000000000000\n"
	     "RCX=0000000000000000  RBP=0000000000000000\n"
	     "RDX=0000000000000000  RSP=%16lx  SS=%04lx\n"
	     " R8=0000000000000000   R9=0000000000000000\n"
	     "R10=0000000000000000  R11=0000000000000000\n"
	     "R12=0000000000000000  R13=0000000000000000\n"
	     "R14=0000000000000000  R15=0000000000000000\n"
	     "invoking user the first time (user level registers)\n",
	     p.top_value(-2), p.top_value(-1) & 0xffff);
      break;
    case Jdb::s_fputrap:
      printf("RAX=%016lx  RSI=----------------\n"
	     "RBX=----------------  RDI=----------------\n"
             "RCX=%016lx  RBP=----------------\n"
	     "RDX=%016lx  RSP=%016lx  SS=%04lx\n"
	     " R8=----------------   R9=----------------\n"
	     "R10=----------------  R11=----------------\n"
	     "R12=----------------  R13=----------------\n"
	     "R14=----------------  R15=----------------\n"
	     "in exception #0x07 (user level registers)\n",
	     p.top_value(-7), p.top_value(-8),
	     p.top_value(-9), p.top_value(-2), p.top_value(-1) & 0xffff);
      break;
    case Jdb::s_pagefault:
      printf("RAX=%016lx  RSI=----------------\n"
	     "RBX=----------------  RDI=----------------\n"
             "RCX=%016lx  RBP=%016lx\n"
	     "RDX=%016lx  RSP=%016lx  SS=%04lx\n"
	     " R8=----------------   R9=----------------\n"
	     "R10=----------------  R11=----------------\n"
	     "R12=----------------  R13=----------------\n"
	     "R14=----------------  R15=----------------\n"
	     "in page fault, error %08lx (user level registers)\n"
	     "\n"
	     "page fault linear address %16lx\n",
	     p.top_value( -7), p.top_value( -8), p.top_value(-6),
	     p.top_value( -9), p.top_value( -2), p.top_value(-1) & 0xffff,
	     p.top_value(-10), p.top_value(-11));
      break;
    case Jdb::s_interrupt:
      printf("RAX=%016lx\n  RSI=----------------\n"
	     "RBX=----------------  RDI=----------------\n"
             "RCX=%016lx\n  RBP=----------------\n"
	     "RDX=%016lx  RSP=%016lx  SS=%04lx\n"
	     " R8=----------------   R9=----------------\n"
	     "R10=----------------  R11=----------------\n"
	     "R12=----------------  R13=----------------\n"
	     "R14=----------------  R15=----------------\n"
	     "in interrupt #0x%02lx (user level registers)\n",
	     p.top_value(-6), p.top_value(-8),
	     p.top_value(-7), p.top_value(-2), p.top_value(-1) & 0xffff,
	     p.top_value(-9));
      break;
    case Jdb::s_timer_interrupt:
      if constexpr (Config::Have_frame_ptr)
	sub = -1;
      printf("RAX=%016lx  RSI=----------------\n"
	     "RBX=----------------  RDI=----------------\n"
             "RCX=%016lx  RBP=----------------\n"
	     "RDX=%016lx  RSP=%016lx  SS=%04lx\n"
	     " R8=----------------   R9=----------------\n"
	     "R10=----------------  R11=----------------\n"
	     "R12=----------------  R13=----------------\n"
	     "R14=----------------  R15=----------------\n"
	     "in timer interrupt (user level registers)\n",
	     p.top_value(-6-sub), p.top_value(-8-sub),
	     p.top_value(-7-sub), p.top_value(-2), p.top_value(-1) & 0xffff);
      break;
    case Jdb::s_slowtrap:
      printf("RAX=%016lx  RSI=%016lx\n"
             "RBX=%016lx  RDI=%016lx\n"
             "RCX=%016lx  RBP=%016lx\n"
             "RDX=%016lx  RSP=%016lx  SS=%04lx\n"
             " R8=%016lx   R9=%016lx\n"
             "R10=%016lx  R11=%016lx\n"
             "R12=%016lx  R13=%016lx\n"
             "R14=%016lx  R15=%016lx  CS=%04lx\n"
             "in exception %lu, error %08lx (user level registers)\n",
	     p.top_value( -8), p.top_value(-14),
	     p.top_value(-11), p.top_value(-15),
	     p.top_value( -9), p.top_value(-13),
	     p.top_value(-10), p.top_value( -2), p.top_value(-1) & 0xffff,
	     p.top_value(-16), p.top_value(-17),
	     p.top_value(-18), p.top_value(-19),
	     p.top_value(-20), p.top_value(-21),
	     p.top_value(-22), p.top_value(-23), p.top_value(-4) & 0xffff,
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
  char reg = char{-1};
  Mword *reg_ptr = nullptr;
  unsigned x=0, y=0;


  Jdb::printf_statline("tcb", nullptr, "edit register "
      "r{ax|bx|cx|dx|si|di|sp|bp|ip|fl}: ");
  Jdb::cursor(Jdb_screen::height(), 53);

  switch (reg)
    {
    case  1:  x= 4;    y= 9;    reg_ptr = &ef->_ax; break;
    case  2:  x= 4;    y=10;    reg_ptr = &ef->_bx; break;
    case  3:  x= 4;    y=11;    reg_ptr = &ef->_cx; break;
    case  4:  x= 4;    y=12;    reg_ptr = &ef->_dx; break;
    case  5:  x=24;    y=11;    reg_ptr = &ef->_bp; break;
    case  6:  x=24;    y= 9;    reg_ptr = &ef->_si; break;
    case  7:  x=24;    y=10;    reg_ptr = &ef->_di; break;
    case  8:  x=13;    y=14;    reg_ptr = &ef->_ip; break;
    case  20: x= 4;    y=14;    reg_ptr = &ef->_ip; break;
    case  21: x=24;    y=14;    reg_ptr = &ef->_ip; break;
    case  22: x= 4;    y=15;    reg_ptr = &ef->_ip; break;
    case  23: x=24;    y=15;    reg_ptr = &ef->_ip; break;
    case  24: x= 4;    y=16;    reg_ptr = &ef->_ip; break;

    case  9: // we have no esp if we come from kernel
	      if (!ef->from_user())
	        return false;
	      x=24;    y=12;    reg_ptr = &ef->_sp;    break;
    case 10:  x=35;    y=12;    reg_ptr = &ef->_flags; break;
    default: return false;
    }

  Jdb::cursor(y+1, x+1);
  putstr("                ");
  Jdb::printf_statline("tcb", nullptr, "edit %s = %16lx", 
      Jdb_screen::Reg_names[reg-1],
      *reg_ptr);
  Jdb::cursor(y+1, x+1);
  if (Jdb_input::get_mword(&value, 16, 16))
    {
      *reg_ptr = value;
      Jdb::cursor(y+1, x+1);
      printf("%016lx", *reg_ptr);
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
  const char *desc[] = { "SS", "SP", "RFL", "CS", "IP" };
  return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
}

IMPLEMENT_OVERRIDE
Address
Jdb_tcb_ptr::user_ip() const
{
  return top_value(-5);
}
