INTERFACE [amd64]:

EXTENSION class Context
{
protected:
  Mword _gs_base, _fs_base;
  Unsigned16 _ds;

public:
  Mword *fs_base() { return &_fs_base; }
  Mword *gs_base() { return &_gs_base; }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

IMPLEMENT inline NEEDS [Context::store_segments]
void
Context::switch_cpu(Context *t)
{
  Mword dummy1, dummy2, dummy3, dummy4;

  t->load_gdt_user_entries(this);

  _ds = Cpu::get_ds();
  if (_ds | t->_ds) [[unlikely]]
    Cpu::set_ds(t->_ds);

  _es = Cpu::get_es();
  if (_es | t->_es) [[unlikely]]
    Cpu::set_es(t->_es);

  _fs = Cpu::get_fs();
  if (_fs | _fs_base | t->_fs) [[unlikely]]
    Cpu::set_fs(t->_fs);

  if (!t->_fs)
    Cpu::set_fs_base(&t->_fs_base);

  _gs = Cpu::get_gs();
  if (_gs | _gs_base | t->_gs) [[unlikely]]
    Cpu::set_gs(t->_gs);

  if (!t->_gs)
    Cpu::set_gs_base(&t->_gs_base);

  store_segments();
  asm volatile
    (
     "   push %%rbp			\n\t"	// save base ptr of old thread
     "   pushq $1f			\n\t"	// push restart addr on old stack
     "   mov  %%rsp, (%[old_ksp])	\n\t"	// save stack pointer
     "   mov  (%[new_ksp]), %%rsp	\n\t"	// load new stack pointer
     // in new context now (cli'd)
     "   call  switchin_context_label	\n\t"	// switch pagetable
     "   pop   %%rax			\n\t"	// don't do ret here -- we want
						// to preserve the return stack
     " .if %c[intel_its_mitigation]	\n\t"
     "   jmp   __x86_indirect_thunk_rax	\n\t"
     " .else				\n\t"
     "   jmp   *%%rax			\n\t"
     " .endif				\n\t"
     // restart code
     "  .p2align 4			\n\t"	// start code at new cache line
     "1: pop %%rbp			\n\t"	// restore base ptr

     : "=c" (dummy1), "=a" (dummy2), "=D" (dummy3), "=S" (dummy4)
     : [old_ksp]    "c" (&_kernel_sp),
       [new_ksp]    "a" (&t->_kernel_sp),
       [new_thread] "D" (t),
       [old_thread] "S" (this),
       [intel_its_mitigation] "i" (TAG_ENABLED(intel_its_mitigation))
     : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rbx", "rdx", "memory");
}

PROTECTED inline
void
Context::arch_setup_utcb_ptr()
{
  _utcb.access()->utcb_addr = reinterpret_cast<Mword>(_utcb.usr().get());
  _gs_base = reinterpret_cast<Address>(&_utcb.usr()->utcb_addr);
}

PROTECTED inline
void
Context::load_segments()
{
}

PROTECTED inline
void
Context::store_segments()
{}

IMPLEMENT inline
void
Context::spill_user_state()
{
  _ds = Cpu::get_ds();
  _es = Cpu::get_es();
  _fs = Cpu::get_fs();
  _gs = Cpu::get_gs();
}

IMPLEMENT inline
void
Context::fill_user_state()
{
  Cpu::set_ds(_ds);
  Cpu::set_es(_es);
  Cpu::set_fs(_fs);
  Cpu::set_gs(_gs);

  if (!_fs) [[likely]]
    Cpu::set_fs_base(&_fs_base);

  if (!_gs) [[likely]]
    Cpu::set_gs_base(&_gs_base);
}

IMPLEMENT_OVERRIDE inline
void
Context::vcpu_pv_switch_to_kernel(Vcpu_state *vcpu, bool current)
{
  _fs_base = access_once(&vcpu->host.fs_base);
  _gs_base = access_once(&vcpu->host.gs_base);

  vcpu->_regs.ds = _ds;
  vcpu->_regs.es = _es;
  vcpu->_regs.fs = _fs;
  vcpu->_regs.gs = _gs;

  unsigned tmp = access_once(&vcpu->host.ds);
  if (current && (_ds | tmp)) [[unlikely]]
    Cpu::set_ds(tmp);
  _ds = tmp;

  tmp = access_once(&vcpu->host.es);
  if (current && (_es | tmp)) [[unlikely]]
    Cpu::set_es(tmp);
  _es = tmp;

  tmp = access_once(&vcpu->host.fs);
  if (current && (_fs | tmp)) [[unlikely]]
    Cpu::set_fs(tmp);
  _fs = tmp;

  if (current && !tmp) [[likely]]
    Cpu::set_fs_base(&_fs_base);

  tmp = access_once(&vcpu->host.gs);
  if (current && (_gs | tmp)) [[unlikely]]
    Cpu::set_gs(tmp);
  _gs = tmp;

  if (current && !tmp) [[likely]]
    Cpu::set_gs_base(&_gs_base);
}

IMPLEMENT_OVERRIDE inline
void
Context::vcpu_pv_switch_to_user(Vcpu_state *vcpu, bool current)
{
  _fs_base = access_once(&vcpu->_regs.fs_base);
  _gs_base = access_once(&vcpu->_regs.gs_base);

  unsigned tmp = access_once(&vcpu->_regs.ds);
  if (current && (_ds | tmp)) [[unlikely]]
    Cpu::set_ds(tmp);
  _ds = tmp;

  tmp = access_once(&vcpu->_regs.es);
  if (current && (_es | tmp)) [[unlikely]]
    Cpu::set_es(tmp);
  _es = tmp;

  tmp = access_once(&vcpu->_regs.fs);
  if (current && (_fs | tmp)) [[unlikely]]
    Cpu::set_fs(tmp);
  _fs = tmp;

  if (current && !tmp) [[likely]]
    Cpu::set_fs_base(&_fs_base);

  tmp = access_once(&vcpu->_regs.gs);
  if (current && (_gs | tmp)) [[unlikely]]
    Cpu::set_gs(tmp);
  _gs = tmp;

  if (current && !tmp) [[likely]]
    Cpu::set_gs_base(&_gs_base);
}
