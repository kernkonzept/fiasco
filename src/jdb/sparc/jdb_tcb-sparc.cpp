IMPLEMENTATION [sparc]:

EXTENSION class Jdb_tcb
{
  enum
  {
    Disasm_x = 41,
    Disasm_y = 11,
    Stack_y  = 20,
  };
};

IMPLEMENT
void Jdb_tcb::print_entry_frame_regs(Thread *)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(Jdb::current_cpu);
  int from_user       = ef->from_user();

  printf("\n\n\nRegisters (before debug entry from %s mode):\n"
         "[0]  %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx\n"
         "[8]  %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx\n"
         "[16] %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx\n"
         "[24] %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx"
         "\033[m\n sp = %08lx\n",
         from_user ? "user" : "kernel",
	 ef->r[0],  ef->usp,   ef->r[1],  ef->r[2],  ef->r[3],
	 ef->r[4],  ef->r[5],  ef->r[6],  ef->r[7],  ef->r[8],
	 ef->r[9],  ef->r11,   ef->r12 ,  ef->r[10], ef->r[11],
	 ef->r[12], ef->r[13], ef->r[14], ef->r[15], ef->r[16],
	 ef->r[17], ef->r[18], ef->r[19], ef->r[20], ef->r[21],
	 ef->r[22], ef->r[23], ef->r[24], ef->r[25], ef->r[26],
	 ef->r[27], ef->r[28], ef->usp);
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("\n\n\nSRR0=%s%08lx\033[m USP=%08lx ULR=%08lx ENTRY IP=%08lx\n",
      Jdb::esc_emph, current.top_value(-5), current.top_value(-3),
      current.top_value(-4), current.top_value(-10));

  printf("[0]  %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx\n"
         "[8]  %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx\n"
         "[16] %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx\n"
         "[24] %08lx %08lx %08lx  %08lx %08lx %08lx %08lx %08lx"
         "\033[m\n",
         current.top_value(-39), //0
         current.top_value(-3),  //1
         current.top_value(-38), //2
         current.top_value(-37), //3
         current.top_value(-36), //4
         current.top_value(-35), //5
         current.top_value(-34), //6
         current.top_value(-33), //7
         current.top_value(-32), //8
         current.top_value(-31), //9
         current.top_value(-30), //10
         current.top_value(-2),  //11
         current.top_value(-1),  //12
         current.top_value(-29), //13
         current.top_value(-28), //14
         current.top_value(-27), //15
         current.top_value(-26), //16
         current.top_value(-25), //17
         current.top_value(-24), //18
         current.top_value(-23), //19
         current.top_value(-22), //20
         current.top_value(-21), //21
         current.top_value(-20), //22
         current.top_value(-19), //23
         current.top_value(-18), //24
         current.top_value(-17), //25
         current.top_value(-16), //26
         current.top_value(-15), //27
         current.top_value(-14), //28
         current.top_value(-13), //29
         current.top_value(-12), //30
         current.top_value(-11) //31
        );
}

IMPLEMENT
void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &, Address)
{}

IMPLEMENT_OVERRIDE
bool
Jdb_stack_view::edit_registers()
{
  return false;
}

IMPLEMENT inline
bool
Jdb_tcb_ptr::is_user_value() const
{
  return _offs >= Context::Size - 9 * sizeof(Mword);
}

IMPLEMENT inline
const char *
Jdb_tcb_ptr::user_value_desc() const
{
  const char *desc[] = { "r12", "r11", "USP", "ULR", "SRR0", "SRR1",
                         "CR", "CTR", "XER" };
  return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
}
