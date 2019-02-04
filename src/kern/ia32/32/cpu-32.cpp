IMPLEMENTATION [ia32]:

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


PUBLIC inline FIASCO_CONST
Unsigned64
Cpu::ns_to_tsc(Unsigned64 ns) const
{
  Unsigned32 dummy;
  Unsigned64 tsc;
  asm inline
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

PUBLIC inline FIASCO_CONST
Unsigned64
Cpu::tsc_to_ns(Unsigned64 tsc) const
{
  Unsigned32 dummy1, dummy2;
  Unsigned64 ns;
  asm inline
	("movl  %%edx, %%ecx		\n\t"
	 "mull	%4			\n\t"
	 "movl  %%eax, %2		\n\t"
	 "movl	%%ecx, %%eax		\n\t"
	 "movl	%%edx, %%ecx		\n\t"
	 "mull	%4			\n\t"
	 "addl	%%ecx, %%eax		\n\t"
	 "adcl	$0, %%edx		\n\t"
	 "shld	$5, %%eax, %%edx	\n\t"
	 "shld  $5, %2, %%eax		\n\t"
	:"=A" (ns), "=&c" (dummy1), "=&r" (dummy2)
	: "0" (tsc), "b" (scaler_tsc_to_ns)
	);
  return ns;
}

PUBLIC inline FIASCO_CONST
Unsigned64
Cpu::tsc_to_us(Unsigned64 tsc) const
{
  Unsigned32 dummy;
  Unsigned64 us;
  asm inline
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
    asm inline
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
  asm inline volatile ("rdtsc" : "=A" (tsc));
  return tsc;
}

/**
 * Support for RDTSCP is indicated by CPUID.80000001H:EDX[27].
 */
PUBLIC static inline
Unsigned64
Cpu::rdtscp()
{
  Unsigned64 tsc;
  asm inline volatile ("rdtscp" : "=A" (tsc) :: "ecx");
  return tsc;
}

PUBLIC static inline
Unsigned32
Cpu::get_flags()
{ Unsigned32 efl; asm inline volatile ("pushfl ; popl %0" : "=r"(efl)); return efl; }

PUBLIC static inline
void
Cpu::set_flags(Unsigned32 efl)
{ asm inline volatile ("pushl %0 ; popfl" : : "rm" (efl) : "memory"); }

IMPLEMENT inline NEEDS["tss.h"]
Address volatile &
Cpu::kernel_sp() const
{ return *reinterpret_cast<Address volatile *>(&get_tss()->_hw.ctx.esp0); }

//----------------------------------------------------------------------------
IMPLEMENTATION[ia32]:

#include "entry-ia32.h"

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
Cpu::init_tss_dbf(Tss *tss_dbf, Address kdir)
{
  _tss_dbf = tss_dbf;

  gdt->set_entry_tss(Gdt::gdt_tss_dbf / 8, reinterpret_cast<Address>(_tss_dbf),
                     Tss::Segment_limit_sans_io);

  _tss_dbf->_hw.ctx.cs     = Gdt::gdt_code_kernel;
  _tss_dbf->_hw.ctx.ss     = Gdt::gdt_data_kernel;
  _tss_dbf->_hw.ctx.ds     = Gdt::gdt_data_kernel;
  _tss_dbf->_hw.ctx.es     = Gdt::gdt_data_kernel;
  _tss_dbf->_hw.ctx.fs     = Gdt::gdt_data_kernel;
  _tss_dbf->_hw.ctx.gs     = Gdt::gdt_data_kernel;
  _tss_dbf->_hw.ctx.eip    = reinterpret_cast<Address>(entry_vec08_dbf);
  _tss_dbf->_hw.ctx.esp    = reinterpret_cast<Address>(&dbf_stack_top);
  _tss_dbf->_hw.ctx.ldt    = 0;
  _tss_dbf->_hw.ctx.eflags = 0x00000082;
  _tss_dbf->_hw.ctx.cr3    = kdir;

  /*
   * Setting the IO bitmap offset beyond the TSS segment limit effectively
   * disables the IO bitmap.
   */
  _tss_dbf->_hw.ctx.iopb   = Tss::Segment_limit_sans_io + 1;
}

PUBLIC FIASCO_INIT_CPU
void
Cpu::init_tss(Tss *tss)
{
  _tss = tss;

  gdt->set_entry_tss(Gdt::gdt_tss / 8, reinterpret_cast<Address>(_tss),
                     Tss::Segment_limit);

  _tss->set_ss0(Gdt::gdt_data_kernel);
  _tss->_hw.io.bitmap_delimiter = 0xffU;

  reset_io_bitmap();
}

PUBLIC FIASCO_INIT_CPU
void
Cpu::init_gdt(Address gdt_mem, Address user_max)
{
  gdt = new (reinterpret_cast<void *>(gdt_mem)) Gdt();

  // make sure kernel cs/ds and user cs/ds are placed in the same
  // cache line, respectively; pre-set all "accessed" flags so that
  // the CPU doesn't need to do this later

  gdt->set_entry_4k(Gdt::gdt_code_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::Kernel, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_data_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::Kernel, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::User, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_data_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::User, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
}

PRIVATE inline
void
Cpu::set_sysenter(void (*func)(void))
{
  _sysenter_eip = reinterpret_cast<Mword>(func);
  wrmsr(reinterpret_cast<Mword>(func), 0, Msr_ia32_sysenter_eip);
}


PUBLIC
void
Cpu::set_fast_entry(void (*func)(void))
{
  if (sysenter())
    set_sysenter(func);
}

PUBLIC inline
void
Cpu::setup_sysenter() const
{
  wrmsr(Gdt::gdt_code_kernel, 0, Msr_ia32_sysenter_cs);
  wrmsr(reinterpret_cast<unsigned long>(&kernel_sp()), 0, Msr_ia32_sysenter_esp);
  wrmsr(_sysenter_eip, 0, Msr_ia32_sysenter_eip);
}

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::init_sysenter()
{
  if (sysenter())
    {
      _sysenter_eip = reinterpret_cast<Mword>(entry_sys_fast_ipc_c);
      setup_sysenter();
    }
}
