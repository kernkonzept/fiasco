INTERFACE [amd64]:

#include "syscall_entry.h"

EXTENSION class Cpu
{
  Syscall_entry _syscall_entry;
};


IMPLEMENTATION[amd64]:

#include "mem_layout.h"
#include "tss.h"

PUBLIC
void
Cpu::set_fast_entry(void (*func)())
{
  _syscall_entry.set_entry(func);
}

PUBLIC inline
void
Cpu::setup_sysenter() const
{
  wrmsr(0, GDT_CODE_KERNEL | ((GDT_CODE_USER32 | 3) << 16), MSR_STAR);
  wrmsr((Unsigned64)&_syscall_entry, MSR_LSTAR);
  wrmsr((Unsigned64)&_syscall_entry, MSR_CSTAR);
  wrmsr(~0ULL, MSR_SFMASK);
}

extern "C" void entry_sys_fast_ipc_c();

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::init_sysenter()
{
  setup_sysenter();
  wrmsr(rdmsr(MSR_EFER) | 1, MSR_EFER);
  _syscall_entry.set_rsp((Address)&kernel_sp());
  set_fast_entry(entry_sys_fast_ipc_c);
}


PUBLIC inline
Unsigned64
Cpu::ns_to_tsc(Unsigned64 ns) const
{
  Unsigned64 tsc, dummy;
  __asm__
      ("                              \n\t"
       "mulq   %3                      \n\t"
       "shrd  $27, %%rdx, %%rax       \n\t"
       :"=a" (tsc), "=d" (dummy)
       :"a" (ns), "r" ((Unsigned64)scaler_ns_to_tsc)
      );
  return tsc;
}

PUBLIC inline
Unsigned64
Cpu::tsc_to_ns(Unsigned64 tsc) const
{
  Unsigned64 ns, dummy;
  __asm__
      ("                               \n\t"
       "mulq   %3                      \n\t"
       "shrd  $27, %%rdx, %%rax       \n\t"
       :"=a" (ns), "=d"(dummy)
       :"a" (tsc), "r" ((Unsigned64)scaler_tsc_to_ns)
      );
  return ns;
}

PUBLIC inline
Unsigned64
Cpu::tsc_to_us(Unsigned64 tsc) const
{
  Unsigned64 ns, dummy;
  __asm__
      ("                               \n\t"
       "mulq   %3                      \n\t"
       "shrd  $32, %%rdx, %%rax       \n\t"
       :"=a" (ns), "=d" (dummy)
       :"a" (tsc), "r" ((Unsigned64)scaler_tsc_to_us)
      );
  return ns;
}


PUBLIC inline
void
Cpu::tsc_to_s_and_ns(Unsigned64 tsc, Unsigned32 *s, Unsigned32 *ns) const
{
  __asm__
      ("                                \n\t"
       "mulq   %3                       \n\t"
       "shrd  $27, %%rdx, %%rax         \n\t"
       "xorq  %%rdx, %%rdx              \n\t"
       "divq  %4                        \n\t"
       :"=a" (*s), "=&d" (*ns)
       : "a" (tsc), "r" ((Unsigned64)scaler_tsc_to_ns),
         "rm"(1000000000ULL)
      );
}

PUBLIC static inline
Unsigned64
Cpu::rdtsc()
{
  Unsigned64 tsc;
  asm volatile (
         "rdtsc				\n\t"
	 "and   %1,%%rax		\n\t"
	 "shl   $32,%%rdx		\n\t"
	 "or	%%rdx,%%rax		\n\t"
	: "=&a" (tsc)
	: "r" (0xffffffffUL)
	:"rdx"
	);
  return tsc;
}


PUBLIC static inline
Unsigned64
Cpu::get_flags()
{
  Unsigned64 efl;
  asm volatile ("pushf ; popq %0" : "=r"(efl));
  return efl;
}


PUBLIC static inline
void
Cpu::set_flags(Unsigned64 efl)
{
  asm volatile ("pushq %0 ; popf" : : "rm" (efl) : "memory");
}


IMPLEMENT inline NEEDS["tss.h"]
Address volatile &
Cpu::kernel_sp() const
{ return *reinterpret_cast<Address volatile *>(&get_tss()->_rsp0); }

PUBLIC static inline
void
Cpu::set_cs()
{
  // XXX have only memory indirect far jmp in 64Bit mode
  asm volatile (
  "movabsq	$1f, %%rax	\n"
  "pushq	%%rbx		\n"
  "pushq	%%rax		\n"
  "lretq 			\n"
  "1: 				\n"
    :
    : "b" (Gdt::gdt_code_kernel | Gdt::Selector_kernel)
    : "rax", "memory");
}


extern "C" Address dbf_stack_top;

PUBLIC FIASCO_INIT_CPU
void
Cpu::init_tss(Address tss_mem, size_t tss_size)
{
  tss = reinterpret_cast<Tss*>(tss_mem);

  gdt->set_entry_tss(Gdt::gdt_tss/8, tss_mem, tss_size,
		     Gdt_entry::Access_kernel | Gdt_entry::Access_tss, 0);

  // XXX setup pointer for clean double fault stack
  tss->_ist1 = (Address)&dbf_stack_top;
  assert(Mem_layout::Io_bitmap - tss_mem
         < (1 << (sizeof(tss->_io_bit_map_offset) * 8)));
  tss->_io_bit_map_offset = Mem_layout::Io_bitmap - tss_mem;
  init_sysenter();
}


PUBLIC FIASCO_INIT_CPU
void
Cpu::init_gdt(Address gdt_mem, Address user_max)
{
  gdt = reinterpret_cast<Gdt*>(gdt_mem);

  // make sure kernel cs/ds and user cs/ds are placed in the same
  // cache line, respectively; pre-set all "accessed" flags so that
  // the CPU doesn't need to do this later

  gdt->set_entry_4k(Gdt::gdt_code_kernel/8, 0, 0xffffffff,
		  Gdt_entry::Access_kernel | 
		  Gdt_entry::Access_code_read |
  		  Gdt_entry::Accessed, Gdt_entry::Long_mode);
  gdt->set_entry_4k(Gdt::gdt_data_kernel/8, 0, 0xffffffff,
		  Gdt_entry::Access_kernel | 
	  	  Gdt_entry::Access_data_write | 
  		  Gdt_entry::Accessed, Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user/8, 0, user_max,
		  Gdt_entry::Access_user | 
		  Gdt_entry::Access_code_read |
		  Gdt_entry::Accessed, Gdt_entry::Long_mode);
  gdt->set_entry_4k(Gdt::gdt_data_user/8, 0, user_max,
		  Gdt_entry::Access_user |
	  	  Gdt_entry::Access_data_write | 
  		  Gdt_entry::Accessed, Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user32/8, 0, user_max,
		  Gdt_entry::Access_user |
		  Gdt_entry::Access_code_read |
		  Gdt_entry::Accessed, Gdt_entry::Size_32);
}


PUBLIC static inline
Mword
Cpu::stack_align(Mword stack)
{ return stack & ~0xf; }

