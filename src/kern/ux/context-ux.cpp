INTERFACE[ux]:

EXTENSION class Context
{
protected:
  enum { Gdt_user_entries = 3 };
  struct { Unsigned64 v[2]; } _gdt_user_entries[Gdt_user_entries]; // type struct Ldt_user_desc

  bool _is_native; // thread can call Linux host system calls
  Unsigned32                  _es, _fs, _gs;
};

// ---------------------------------------------------------------------
IMPLEMENTATION[ux]:

#include "utcb_init.h"

IMPLEMENT inline
void
Context::switch_fpu (Context *)
{}

PUBLIC inline
void
Context::spill_fpu()
{}

PUBLIC inline
bool
Context::is_native()
{ return _is_native; }

PROTECTED inline NEEDS["utcb_init.h"]
void
Context::arch_setup_utcb_ptr()
{
  _gs = _fs = Utcb_init::utcb_segment();
}

PROTECTED inline
void
Context::load_gdt_user_entries(Context *old = 0)
{
  Mword *trampoline_page = (Mword *) Kmem::phys_to_virt(Mem_layout::Trampoline_frame);
  Space *tos = vcpu_aware_space();

  if (EXPECT_FALSE(!tos))
    return;

  for (int i = 0; i < 3; i++)
    if (!old || old == this
	|| old->_gdt_user_entries[i].v[0] != _gdt_user_entries[i].v[0]
        || old->_gdt_user_entries[i].v[1] != _gdt_user_entries[i].v[1])
      {
        memcpy(trampoline_page + 1, &_gdt_user_entries[i],
               sizeof(_gdt_user_entries[0]));
        Trampoline::syscall(tos->pid(), 243,
                            Mem_layout::Trampoline_page + sizeof(Mword));
      }

  // update the global UTCB pointer to make the thread find its UTCB
  // using fs:[0]
  Mem_layout::user_utcb_ptr(current_cpu()) = utcb().usr();
}
