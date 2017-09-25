IMPLEMENTATION [mips]:

EXTENSION class Jdb_tcb
{
  enum
  {
    Disasm_x = 41,
    Disasm_y = 11,
    Stack_y  = 20,
  };
};

IMPLEMENT inline
bool
Jdb_tcb_ptr::is_user_value() const
{
  return _offs >= Context::Size - sizeof(Trap_state);
}

IMPLEMENT inline
const char *
Jdb_tcb_ptr::user_value_desc() const
{
  const char *const desc[] =
    {
      "Epc", "Status", "Cause", "BadVAddr", "Lo", "Hi",
      "ra ($31)", "s8 ($30)", "sp ($29)", "gp ($28)",
      "k1 ($27)", "k0 ($26)", "t9 ($25)", "t8 ($24)",
      "s7 ($23)", "s6 ($22)", "s5 ($21)", "s4 ($s0)",
      "s3 ($19)", "s2 ($18)", "s1 ($17)", "s0 ($16)",
      "t7 ($15)", "t6 ($14)", "t5 ($13)", "t4 ($12)",
      "t3 ($11)", "t2 ($10)", "t1 ($9)",  "t0 ($8)",
      "a3 ($7)",  "a2 ($6)",  "a1 ($5)",  "a0 ($4)",
      "v1 ($3)",  "v0 ($2)",  "at ($1)",  "eret-work",
      "BadInstr", "BadInstrP"
    };
  static_assert ((sizeof (Trap_state) / sizeof (Mword))
                 <= (sizeof (desc) / sizeof (desc[0])),
                 "desc entries do not match the sizeof Trap_state");
  return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
}

IMPLEMENT
void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &, Address)
{}

IMPLEMENT
void Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(Jdb::current_cpu);
  int from_user       = ef->from_user();
  Mem_space *s = t->mem_space();

  printf("\n\n\nRegisters (before debug entry from %s mode):\n"
         "[0]  %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "[8]  %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "[16] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "[24] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "Hi=%08lx Lo=%08lx Pf=%08lx Ca=%08lx St=%08lx Epc=%08lx Asid=%lx\n",
         from_user ? "user" : "kernel",
         ef->r[0],  ef->r[1], ef->r[2], ef->r[3], ef->r[4],
         ef->r[5],  ef->r[6], ef->r[7], ef->r[8], ef->r[9],
         ef->r[10], ef->r[11], ef->r[12], ef->r[13], ef->r[14],
         ef->r[15], ef->r[16], ef->r[17], ef->r[18], ef->r[19],
         ef->r[20], ef->r[21], ef->r[22], ef->r[23], ef->r[24],
         ef->r[25], ef->r[26], ef->r[27], ef->r[28], ef->r[29],
         ef->r[30], ef->r[31], ef->hi, ef->lo, ef->bad_v_addr,
         ef->cause, ef->status, ef->epc, s->c_asid());
}

IMPLEMENT
void
Jdb_tcb::info_thread_state(Thread *t)
{
  Mem_space *s = t->mem_space();

  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("\nCause=%08lx Status=%08lx Epc=%08lx BadVaddr=%08lx\n"
         "Asid=%lx Hi=%lx Lo=%lx\n",
         current.top_value(-3), current.top_value(-2), current.top_value(-1),
         current.top_value(-4), s->c_asid(), current.top_value(-5),
         current.top_value(-6));

  for (unsigned i = 0; i < 32; ++i)
    {
      if ((i & 7) == 0)
        printf("[%2u] ", i);

      printf("%08lx%s", current.top_value(-38 + i),
             ((i & 7) == 7) ? "\n" : (((i & 7) == 3) ? "  " : " "));
    }
}


