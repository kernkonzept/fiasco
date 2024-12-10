IMPLEMENTATION[ia32]:

PROTECTED inline
void
Context::arch_setup_utcb_ptr()
{
  _utcb.access()->utcb_addr = reinterpret_cast<Mword>(_utcb.usr().get());
  _gdt_user_entries[Gdt_user_entries]
    = Gdt_entry(reinterpret_cast<Address>(&_utcb.usr()->utcb_addr), 0xfffff,
                Gdt_entry::Accessed, Gdt_entry::Data_write, Gdt_entry::User,
                Gdt_entry::Code_undef, Gdt_entry::Size_32,
                Gdt_entry::Granularity_4k);
  _gs = _fs = Gdt::gdt_utcb | 3;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [ia32]:

IMPLEMENT inline
void
Context::spill_user_state()
{}

IMPLEMENT inline
void
Context::fill_user_state()
{}

IMPLEMENT inline NEEDS [Context::update_consumed_time,
			Context::store_segments]
void
Context::switch_cpu(Context *t)
{
  Mword dummy1, dummy2, dummy3, dummy4;

  store_segments();

  t->load_gdt_user_entries(this);

  asm volatile
    (
     "   pushl %%ebp			\n\t"	// save base ptr of old thread
     "   pushl $1f			\n\t"	// restart addr to old stack
     "   movl  %%esp, (%0)		\n\t"	// save stack pointer
     "   movl  (%1), %%esp		\n\t"	// load new stack pointer
     						// in new context now (cli'd)
     "   movl  %2, %%eax		\n\t"	// new thread's "this"
     "   call  switchin_context_label	\n\t"	// switch pagetable
     "   popl  %%eax			\n\t"	// don't do ret here -- we want
     "   jmp   *%%eax			\n\t"	// to preserve the return stack
						// restart code
     "  .p2align 4			\n\t"	// start code at new cache line
     "1: popl %%ebp			\n\t"	// restore base ptr

     : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3), "=d" (dummy4)
     : "c" (&_kernel_sp), "S" (&t->_kernel_sp), "D" (t), "d" (this)
     : "eax", "ebx", "memory");
}
