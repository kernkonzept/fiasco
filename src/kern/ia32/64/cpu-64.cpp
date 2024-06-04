INTERFACE [amd64 && !kernel_isolation]:

#include "syscall_entry.h"

EXTENSION class Cpu
{
  static Per_cpu_array<Syscall_entry_data>
    _syscall_entry_data asm("syscall_entry_data");
};


IMPLEMENTATION[amd64 && !kernel_isolation]:

#include "mem_layout.h"
#include "tss.h"

Per_cpu_array<Syscall_entry_data> Cpu::_syscall_entry_data;

IMPLEMENT inline NEEDS["tss.h"]
Address volatile &
Cpu::kernel_sp() const
{ return *reinterpret_cast<Address volatile *>(&get_tss()->_hw.ctx.rsp0); }

PUBLIC inline
void
Cpu::setup_sysenter()
{
  extern Per_cpu_array<Syscall_entry_text> syscall_entry_text;

  wrmsr(0, GDT_CODE_KERNEL | ((GDT_CODE_USER32 | 3) << 16), MSR_STAR);
  wrmsr(reinterpret_cast<Unsigned64>(&syscall_entry_text[id()]), MSR_LSTAR);
  wrmsr(reinterpret_cast<Unsigned64>(&syscall_entry_text[id()]), MSR_CSTAR);
  wrmsr(~0U, MSR_SFMASK);
  _syscall_entry_data[id()].set_rsp(reinterpret_cast<Address>(&kernel_sp()));
}

IMPLEMENTATION[amd64 && kernel_isolation]:

#include "mem_layout.h"
#include "tss.h"

PUBLIC inline
void
Cpu::setup_sysenter() const
{
  wrmsr(0, GDT_CODE_KERNEL | ((GDT_CODE_USER32 | 3) << 16), MSR_STAR);
  wrmsr(Mem_layout::Kentry_cpu_syscall_entry, MSR_LSTAR);
  wrmsr(Mem_layout::Kentry_cpu_syscall_entry,
        MSR_CSTAR);
  wrmsr(~0U, MSR_SFMASK);
}

IMPLEMENT inline NEEDS["mem_layout.h"]
Address volatile &
Cpu::kernel_sp() const
{ return *reinterpret_cast<Address volatile *>(Mem_layout::Kentry_cpu_page + sizeof(Mword)); }

IMPLEMENTATION[amd64]:

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::init_sysenter()
{
  setup_sysenter();
  wrmsr(rdmsr(MSR_EFER) | 1, MSR_EFER);
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
       :"a" (ns), "r" (Unsigned64{scaler_ns_to_tsc})
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
       :"a" (tsc), "r" (Unsigned64{scaler_tsc_to_ns})
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
       :"a" (tsc), "r" (Unsigned64{scaler_tsc_to_us})
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
       : "a" (tsc), "r" (Unsigned64{scaler_tsc_to_ns}),
         "rm"(1000000000ULL)
      );
}

PUBLIC static inline
Unsigned64
Cpu::rdtsc()
{
  Unsigned64 h, l;
  asm volatile ("rdtsc" : "=a" (l), "=d" (h));
  return (h << 32) | l;
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

PUBLIC static inline NEEDS["asm.h"]
void
Cpu::set_fs_gs_base(Mword *base, Mword reg)
{
  asm volatile (
    "2: movq\t%0, %%rax\n\t"
    "   movq\t%%rax, %%rdx\n\t"
    "   shrq\t$32, %%rdx\n\t"
    "1: wrmsr\n\t"
    ".pushsection\t\".fixup.%=\", \"ax?\"\n\t"
    "3: movq\t$0, %0\n\t"
    "   jmp\t2b\n\t"
    ".popsection\n\t"
    ASM_KEX(1b, 3b)
     : "+m" (*base)
     : "c" (reg) : "rax", "rdx");
}

PUBLIC static inline NEEDS["asm.h"]
void
Cpu::set_canonical_msr(Unsigned64 value, Mword reg)
{
  asm volatile (
    "2: movq\t%%rax, %%rdx\n\t"
    "   shrq\t$32, %%rdx\n\t"
    "1: wrmsr\n\t"
    ".pushsection\t\".fixup.%=\", \"ax?\"\n\t"
    "3: movq\t$0, %%rax\n\t"
    "   jmp\t2b\n\t"
    ".popsection\n\t"
    ASM_KEX(1b, 3b)
     : "+a" (value)
     : "c" (reg) : "rdx");
}

PUBLIC static inline NEEDS[Cpu::set_fs_gs_base]
void
Cpu::set_fs_base(Mword *base)
{
  set_fs_gs_base(base, MSR_FS_BASE);
}

PUBLIC static inline NEEDS[Cpu::set_fs_gs_base]
void
Cpu::set_gs_base(Mword *base)
{
  set_fs_gs_base(base, MSR_GS_BASE);
}

extern "C" Address dbf_stack_top;

PUBLIC FIASCO_INIT_CPU
void
Cpu::init_tss(Tss *tss)
{
  _tss = tss;

  gdt->set_entry_tss(Gdt::gdt_tss / 8, reinterpret_cast<Address>(_tss),
                     Tss::Segment_limit);

  // XXX setup pointer for clean double fault stack
  _tss->_hw.ctx.ist1 = reinterpret_cast<Address>(&dbf_stack_top);
  _tss->_hw.io.bitmap_delimiter = 0xffU;

  reset_io_bitmap();
  init_sysenter();
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
                    Gdt_entry::Kernel, Gdt_entry::Code_64bit,
                    Gdt_entry::Size_undef);
  gdt->set_entry_4k(Gdt::gdt_data_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::Kernel, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::User, Gdt_entry::Code_64bit,
                    Gdt_entry::Size_undef);
  gdt->set_entry_4k(Gdt::gdt_data_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::User, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user32 / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::User, Gdt_entry::Code_compat,
                    Gdt_entry::Size_32);
}

PUBLIC static inline
Mword
Cpu::stack_align(Mword stack)
{ return stack & ~0xf; }

IMPLEMENT_OVERRIDE inline
bool
Cpu::is_canonical_address(Address addr)
{ return (addr >= (~0UL << 47)) || (addr <= (~0UL >> 17)); }
