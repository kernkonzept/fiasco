IMPLEMENTATION [ia32,ux]:

#include "mem_layout.h"
#include "tss.h"

EXTENSION class Cpu
{
  Mword _sysenter_eip;
};

PUBLIC static inline
Mword
Cpu::stack_align(Mword stack)
{ return stack & ~0x3; }


PUBLIC inline
Unsigned64
Cpu::ns_to_tsc(Unsigned64 ns) const
{
  Unsigned32 dummy;
  Unsigned64 tsc;
  asm volatile
	("movl  %%edx, %%ecx		\n\t"
	 "mull	%3			\n\t"
	 "movl	%%ecx, %%eax		\n\t"
	 "movl	%%edx, %%ecx		\n\t"
	 "mull	%3			\n\t"
	 "addl	%%ecx, %%eax		\n\t"
	 "adcl	$0, %%edx		\n\t"
	 "shld	$5, %%eax, %%edx	\n\t"
	 "shll	$5, %%eax		\n\t"
	:"=A" (tsc), "=&c" (dummy)
	: "0" (ns),  "b" (scaler_ns_to_tsc)
	);
  return tsc;
}

PUBLIC inline
Unsigned64
Cpu::tsc_to_ns(Unsigned64 tsc) const
{
  Unsigned32 dummy;
  Unsigned64 ns;
  asm volatile
	("movl  %%edx, %%ecx		\n\t"
	 "mull	%3			\n\t"
	 "movl	%%ecx, %%eax		\n\t"
	 "movl	%%edx, %%ecx		\n\t"
	 "mull	%3			\n\t"
	 "addl	%%ecx, %%eax		\n\t"
	 "adcl	$0, %%edx		\n\t"
	 "shld	$5, %%eax, %%edx	\n\t"
	 "shll	$5, %%eax		\n\t"
	:"=A" (ns), "=&c" (dummy)
	: "0" (tsc), "b" (scaler_tsc_to_ns)
	);
  return ns;
}

PUBLIC inline
Unsigned64
Cpu::tsc_to_us(Unsigned64 tsc) const
{
  Unsigned32 dummy;
  Unsigned64 us;
  asm volatile
	("movl  %%edx, %%ecx		\n\t"
	 "mull	%3			\n\t"
	 "movl	%%ecx, %%eax		\n\t"
	 "movl	%%edx, %%ecx		\n\t"
	 "mull	%3			\n\t"
	 "addl	%%ecx, %%eax		\n\t"
	 "adcl	$0, %%edx		\n\t"
	:"=A" (us), "=&c" (dummy)
	: "0" (tsc), "S" (scaler_tsc_to_us)
	);
  return us;
}


PUBLIC inline
void
Cpu::tsc_to_s_and_ns(Unsigned64 tsc, Unsigned32 *s, Unsigned32 *ns) const
{
    Unsigned32 dummy;
    __asm__
	("				\n\t"
	 "movl  %%edx, %%ecx		\n\t"
	 "mull	%4			\n\t"
	 "movl	%%ecx, %%eax		\n\t"
	 "movl	%%edx, %%ecx		\n\t"
	 "mull	%4			\n\t"
	 "addl	%%ecx, %%eax		\n\t"
	 "adcl	$0, %%edx		\n\t"
	 "movl  $1000000000, %%ecx	\n\t"
	 "shld	$5, %%eax, %%edx	\n\t"
	 "shll	$5, %%eax		\n\t"
	 "divl  %%ecx			\n\t"
	:"=a" (*s), "=d" (*ns), "=&c" (dummy)
	: "A" (tsc), "g" (scaler_tsc_to_ns)
	);
}


PUBLIC static inline
Unsigned64
Cpu::rdtsc()
{
  Unsigned64 tsc;
  asm volatile ("rdtsc" : "=A" (tsc));
  return tsc;
}

PUBLIC static inline
Unsigned32
Cpu::get_flags()
{ Unsigned32 efl; asm volatile ("pushfl ; popl %0" : "=r"(efl)); return efl; }

PUBLIC static inline
void
Cpu::set_flags(Unsigned32 efl)
{ asm volatile ("pushl %0 ; popfl" : : "rm" (efl) : "memory"); }

IMPLEMENT inline NEEDS["tss.h"]
Address volatile &
Cpu::kernel_sp() const
{ return *reinterpret_cast<Address volatile *>(&get_tss()->_esp0); }

//----------------------------------------------------------------------------
IMPLEMENTATION[ia32]:

PUBLIC static inline
void
Cpu:: set_cs()
{
  asm volatile("ljmp %0, $1f ; 1:"
               : : "i"(Gdt::gdt_code_kernel | Gdt::Selector_kernel));
}

extern "C" void entry_vec08_dbf();
extern "C" Address dbf_stack_top;

PUBLIC FIASCO_INIT_CPU
void
Cpu::init_tss_dbf(Address tss_dbf_mem, Address kdir)
{
  tss_dbf = reinterpret_cast<Tss*>(tss_dbf_mem);

  gdt->set_entry_byte(Gdt::gdt_tss_dbf / 8, tss_dbf_mem, sizeof(Tss) - 1,
		      Gdt_entry::Access_kernel | Gdt_entry::Access_tss |
		      Gdt_entry::Accessed, 0);

  tss_dbf->_cs     = Gdt::gdt_code_kernel;
  tss_dbf->_ss     = Gdt::gdt_data_kernel;
  tss_dbf->_ds     = Gdt::gdt_data_kernel;
  tss_dbf->_es     = Gdt::gdt_data_kernel;
  tss_dbf->_fs     = Gdt::gdt_data_kernel;
  tss_dbf->_gs     = Gdt::gdt_data_kernel;
  tss_dbf->_eip    = (Address)entry_vec08_dbf;
  tss_dbf->_esp    = (Address)&dbf_stack_top;
  tss_dbf->_ldt    = 0;
  tss_dbf->_eflags = 0x00000082;
  tss_dbf->_cr3    = kdir;
  tss_dbf->_io_bit_map_offset = 0x8000;
}


PUBLIC FIASCO_INIT_CPU
void
Cpu::init_tss(Address tss_mem, size_t tss_size)
{
  tss = reinterpret_cast<Tss*>(tss_mem);

  gdt->set_entry_byte(Gdt::gdt_tss / 8, tss_mem, tss_size,
		      Gdt_entry::Access_kernel | Gdt_entry::Access_tss, 0);

  tss->set_ss0(Gdt::gdt_data_kernel);
  tss->_io_bit_map_offset = Mem_layout::Io_bitmap - tss_mem;
}


PUBLIC FIASCO_INIT_CPU
void
Cpu::init_gdt(Address gdt_mem, Address user_max)
{
  gdt = reinterpret_cast<Gdt*>(gdt_mem);

  // make sure kernel cs/ds and user cs/ds are placed in the same
  // cache line, respectively; pre-set all "accessed" flags so that
  // the CPU doesn't need to do this later

  gdt->set_entry_4k(Gdt::gdt_code_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Access_kernel |
                    Gdt_entry::Access_code_read |
                    Gdt_entry::Accessed, Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_data_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Access_kernel |
                    Gdt_entry::Access_data_write |
                    Gdt_entry::Accessed, Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user / 8, 0, user_max,
		    Gdt_entry::Access_user |
		    Gdt_entry::Access_code_read |
		    Gdt_entry::Accessed, Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_data_user / 8, 0, user_max,
                    Gdt_entry::Access_user |
                    Gdt_entry::Access_data_write |
                    Gdt_entry::Accessed, Gdt_entry::Size_32);
}

PRIVATE inline
void
Cpu::set_sysenter(void (*func)(void))
{
  _sysenter_eip = (Mword) func;
  wrmsr((Mword) func, 0, MSR_SYSENTER_EIP);
}


PUBLIC
void
Cpu::set_fast_entry(void (*func)(void))
{
  if (sysenter())
    set_sysenter(func);
}

extern "C" void entry_sys_fast_ipc_c (void);

PUBLIC inline
void
Cpu::setup_sysenter() const
{
  wrmsr(Gdt::gdt_code_kernel, 0, MSR_SYSENTER_CS);
  wrmsr((unsigned long)&kernel_sp(), 0, MSR_SYSENTER_ESP);
  wrmsr(_sysenter_eip, 0, MSR_SYSENTER_EIP);
}

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::init_sysenter()
{
  if (sysenter())
    {
      _sysenter_eip = (Mword)entry_sys_fast_ipc_c;
      setup_sysenter();
    }
}

