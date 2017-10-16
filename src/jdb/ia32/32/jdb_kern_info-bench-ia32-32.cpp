IMPLEMENTATION:

#include <cstdio>
#include "cpu.h"
#include "div32.h"
#include "gdt.h"
#include "simpleio.h"
#include "static_init.h"
#include "timer_tick.h"

static void
Jdb_kern_info_bench::show_time(Unsigned64 time, Unsigned32 rounds,
			       const char *descr)
{
  Unsigned64 cycs = div32(time, rounds);
  printf("  %-24s %6llu.%llu cycles\n",
      descr, cycs, div32(time-cycs*rounds, rounds/10));
}

IMPLEMENT inline
Unsigned64
Jdb_kern_info_bench::get_time_now()
{ return Cpu::rdtsc(); }

#define inst_wbinvd							\
  asm volatile ("wbinvd")

#define inst_invlpg							\
  asm volatile ("invlpg %0" 						\
		: : "m" (*(char*)Mem_layout::Kmem_tmp_page_1))

#define inst_read_cr3							\
  asm volatile ("mov %%cr3,%0"						\
		: "=r"(dummy))

#define inst_reload_cr3							\
  asm volatile ("mov %%cr3,%0; mov %0,%%cr3"				\
		: "=r"(dummy))

#define inst_clts							\
  asm volatile ("clts")

#define inst_cli_sti							\
  asm volatile ("cli; sti")

#define inst_set_cr0_ts							\
  asm volatile ("mov %%cr0,%0; or %1,%0; mov %0,%%cr0"		\
		: "=r" (dummy) : "i" (CR0_TS))

#define inst_push_pop							\
  asm volatile ("push %eax; pop %eax")

#define inst_pushf_pop							\
  asm volatile ("pushf; pop %%eax" : : : "eax")

#define inst_in8_pic							\
  asm volatile ("inb $0x21, %%al" : "=a" (dummy))

#define inst_in8_80							\
  asm volatile ("inb $0x80, %%al" : "=a" (dummy))

#define inst_out8_pic							\
  asm volatile ("outb %%al, $0x21" : : "a" (0xff))

#define inst_apic_timer_read						\
  asm volatile ("" : : "r"(Apic::timer_reg_read()))

#define BENCH(name, instruction, rounds)				\
  do									\
    {									\
      time = Cpu::rdtsc();						\
      for (i=rounds; i; i--)						\
        instruction;							\
      time = Cpu::rdtsc() - time;					\
      show_time (time, rounds, name);					\
    } while (0)


#include "apic.h"
#include "cpu.h"

IMPLEMENT
void
Jdb_kern_info_bench::show_arch()
{
  Unsigned64 time;
  Mword dummy;
  Mword cr0, pic;
  Unsigned32 time_reload_cr3, time_invlpg;
  int i;
  Gdt *gdt = Cpu::boot_cpu()->get_gdt();
  Unsigned32 flags = Proc::cli_save();

  if (!Cpu::boot_cpu()->tsc())
    return;

  // we need a cached, non-global mapping for measuring the time to load
  // TLB entries
  Address phys = Kmem::virt_to_phys((void*)Mem_layout::Tbuf_status_page);
  Kmem::map_phys_page(phys, Mem_layout::Jdb_bench_page, true, false);

    {
      time = Cpu::rdtsc();
      asm volatile ("mov $10000000,%%ecx; .align 4;\n\t"
		    "1:dec %%ecx; jnz 1b" : : : "ecx");
      time = Cpu::rdtsc() - time;
      show_time(time, 10000000, "1:dec ECX, jnz 1b");
    }
  BENCH("wbinvd",		inst_wbinvd,         5000);
  BENCH("invlpg",		inst_invlpg,       200000);
  time_invlpg = time;
  BENCH("read CR3",		inst_read_cr3,     200000);
  BENCH("reload CR3",		inst_reload_cr3,   200000);
  time_reload_cr3 = time;
  cr0 = Cpu::get_cr0();
  BENCH("clts",			inst_clts,         200000);
  BENCH("cli + sti",		inst_cli_sti,      200000);
  Proc::cli();
  BENCH("set CR0.ts",		inst_set_cr0_ts,   200000);
  Cpu::set_cr0(cr0);
  pic = Io::in8(0x21);
  BENCH("in8(PIC)",		inst_in8_pic,      200000);
  BENCH("in8(iodelay)",		inst_in8_80,       200000);
  BENCH("out8(PIC)",		inst_out8_pic,     200000);
  Io::out8(pic, 0x21);
    {
      // read ES segment descriptor
      time = Cpu::rdtsc();
      asm volatile ("mov $200000,%%ebx\n\t"
		    ".align 8; 1: mov %%es, %%eax; dec %%ebx; jnz 1b"
		    : : : "eax", "ebx");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "get ES");
    }
    {
      // set ES segment descriptor and access memory through the ES segment
      time = Cpu::rdtsc();
      asm volatile ("mov  $200000,%%ebx		\n\t"
		    ".align 8			\n\t"
		    "1:				\n\t"
		    "mov  %%eax, %%es		\n\t"
		    "dec  %%ebx			\n\t"
		    "mov  %%es:(%c1),%%edi	\n\t"
		    "jnz  1b			\n\t"
		    :
		    : "a"(Gdt::gdt_data_kernel | Gdt::Selector_kernel),
		      "i"(Mem_layout::Kernel_image)
		    : "ebx", "edi");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "set ES + load ES:mem");
    }
    {
      // write a GDT entry, set ES segment descriptor to this gdt entry
      // and access memory through the ES segment
      Unsigned32 *gdt_e = (Unsigned32*)&(*gdt)[Gdt::gdt_data_kernel/8];
      time = Cpu::rdtsc();
      asm volatile ("mov  $200000,%%ebx		\n\t"
		    ".align 16			\n\t"
		    "1:				\n\t"
		    "mov  %%eax, (%%esi)	\n\t"
		    "mov  %%ecx, 4(%%esi)	\n\t"
		    "mov  %%edx, %%es		\n\t"
		    "dec  %%ebx			\n\t"
		    "mov  %%es:(%c4),%%edi	\n\t"
		    "jnz  1b			\n\t"
		    :
		    : "a"(gdt_e[0]), "c"(gdt_e[1]),
		      "d"(Gdt::gdt_data_kernel | Gdt::Selector_kernel),
		      "S"(gdt->entries() + Gdt::gdt_data_kernel/8),
		      "i"(Mem_layout::Kernel_image)
		    : "ebx", "edi");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "modify ES + load ES:mem");
    }
    {
      // Write "1: int 0x1f ; jmp 1b" to the last 4 bytes of the Tbuf status
      // page (which is accessible by user mode), set IDT entry #0x1f, set
      // TSS.esp0 and do "iret ; int 0x1f". Take care that the user code
      // segment is loaded with limit = 0xffffffff if small spaces are active
      Idt::set_writable(true);
      Gdt_entry orig = (*gdt)[Gdt::gdt_code_user/8];
      Unsigned32 *gdt_e = (Unsigned32*)&(*gdt)[Gdt::gdt_code_user/8];
      gdt_e[0] = 0x0000FFFF;
      gdt_e[1] = 0x00CFFB00;
      asm volatile ("pushf			\n\t"
		    "push %c3			\n\t" // save IDT entry low
		    "push %c3+4			\n\t" // save IDT entry high
		    "push %%ds			\n\t"
		    "push %%es			\n\t" 
		    "push %%fs			\n\t" 
		    "push %%gs			\n\t" 
		    "mov  %%esp, %%edi		\n\t" // save esp
		    "andl $0xffffffc0, %%esp	\n\t" // cache line align esp
		    "movl $(%c4*256+0xfceb00cd), %c2 \n\t"
		    "mov  $1f, %%eax		\n\t" // '1: int 0x1f; jmp 1b'
		    "and  $0x0000ffff, %%eax	\n\t"
		    "or   $0x00080000, %%eax	\n\t"
		    "mov  %%eax, %c3		\n\t" // change IDT entry low
		    "mov  $1f, %%eax		\n\t"
		    "and  $0xffff0000, %%eax	\n\t"
		    "or   $0x0000ee00, %%eax	\n\t"
		    "mov  %%eax, %c3+4		\n\t" // change IDT entry high
		    "mov  (%%ecx), %%ebx	\n\t" // save kernel esp
		    "push $%c5			\n\t" // user ss
		    "push $0			\n\t" // user esp
		    "push $0x3082		\n\t" // user eflags (IOPL3)
		    "push $%c6			\n\t" // user cs
		    "push $%c2			\n\t" // user eip
		    "mov  %%esp, (%%ecx)	\n\t" // change kernel esp
		    "mov  $%c5, %%eax		\n\t"
		    "mov  %%eax, %%ds		\n\t" // ensure dpl3 at ds
		    "mov  %%eax, %%es		\n\t" // ensure dpl3 at es
		    "mov  %%eax, %%fs		\n\t" // ensure dpl3 at fs
		    "mov  %%eax, %%gs		\n\t" // enusre dpl3 at gs

		    "rdtsc			\n\t"
		    "mov  %%eax, %%esi		\n\t"
		    "movl $200001, %%eax	\n\t"

		    ".align 4			\n\t"
		    "1:				\n\t"
		    "dec  %%eax			\n\t"
		    "jz   2f			\n\t"
		    "iret			\n\t"

		    "2:				\n\t"
		    "rdtsc			\n\t"
		    "mov  %%edi, %%esp		\n\t" // restore esp
		    "sub  %%esi, %%eax		\n\t"
		    "pop  %%gs			\n\t"
		    "pop  %%fs			\n\t"
		    "pop  %%es			\n\t"
		    "pop  %%ds			\n\t"
		    "sub  %%edx, %%edx		\n\t"
		    "mov  %%ebx, (%%ecx)	\n\t" // restore kernel esp
		    "pop  %c3+4			\n\t" // restore IDT entry high
		    "pop  %c3			\n\t" // restore IDT entry low
		    "popf			\n\t"

		    : "=A"(time),"=c"(dummy)
		    : "i"(Mem_layout::Tbuf_status_page + Config::PAGE_SIZE-4),
		      "i"(Mem_layout::Idt + (0x1f<<3)),
		      "i"(0x1f),
		      "i"(Gdt::gdt_data_user | Gdt::Selector_user),
		      "i"(Gdt::gdt_code_user | Gdt::Selector_user),
		      "c"((Address)Cpu::boot_cpu()->kernel_sp())
		    : "ebx", "esi", "edi", "memory");
      Idt::set_writable(false);
      (*gdt)[Gdt::gdt_code_user/8] = orig;
      show_time(time, 200000, "int + iret");
    }
  if (Cpu::have_sysenter())
    {
      // Set Sysenter MSR, write "sysenter" to the last two bytes of the
      // Tbuf status page (which is accessible from usermode) and do "sysexit ;
      // sysenter". Gdt doesn't care us since sysexit loads a flat segment.
      // The sysenter esp also doesn't care us since we don't access esp
      // inside the measurement loop.
      asm volatile ("pushf			\n\t"
		    "mov  $0x176, %%ecx		\n\t"
		    "rdmsr			\n\t"
		    "mov  %%eax, %%ebx		\n\t" // save sysenter eip
		    "mov  $1f, %%eax		\n\t"
		    "wrmsr			\n\t" // change sysenter eip
		    "mov  %%esp, %%edi		\n\t" // save esp
		    "movw $0x340f, %c1		\n\t"
		    "rdtsc			\n\t"
		    "mov  %%eax, %%esi		\n\t"
		    "movl $200001, %%eax	\n\t"
		    "mov  $%c1, %%edx		\n\t"

		    ".align 8			\n\t"
		    "1:				\n\t"
		    "dec  %%eax			\n\t"
		    "jz   2f			\n\t"
		    "sysexit			\n\t"

		    "2:				\n\t"
		    "rdtsc			\n\t"
		    "sub  %%eax, %%esi		\n\t"
		    "mov  $0x176, %%ecx		\n\t"
		    "mov  %%ebx, %%eax		\n\t"
		    "sub  %%edx, %%edx		\n\t"
		    "wrmsr			\n\t"

		    "mov  %%esi, %%eax		\n\t"
		    "neg  %%eax			\n\t"
		    "mov  %%edi, %%esp		\n\t" // restore esp
		    "popf			\n\t"
		    : "=A"(time)
		    : "i"(Mem_layout::Tbuf_status_page + Config::PAGE_SIZE-2)
		    : "ebx", "ecx", "esi", "edi", "memory");
      show_time(time, 200000, "sysenter + sysexit");
    }
  if (Cpu::have_sysenter())
    {
      // Set Sysenter MSR, write "sysenter" to the last two bytes of the
      // Tbuf status page (which is accessible from usermode) and do "iret ;
      // sysenter".
      asm volatile ("pushf			\n\t"
		    "push %%ebp			\n\t"
		    "push %%ds			\n\t"
		    "push %%es			\n\t"
		    "push %%fs			\n\t"
		    "push %%gs			\n\t"
		    "mov  $0x176, %%ecx		\n\t"
		    "rdmsr			\n\t"
		    "mov  %%eax, %%ebx		\n\t" // save sysenter eip
		    "mov  $1f, %%eax		\n\t"
		    "wrmsr			\n\t" // change sysenter eip
		    "mov  %%esp, %%edi		\n\t" // save esp
		    "andl $0xffffffc0, %%esp	\n\t" // cache line align esp
		    "movw $0x340f, %c1		\n\t" // 'sysenter'
		    "push $%c3			\n\t" // user ss
		    "push $0			\n\t" // user esp
		    "push $0x3082		\n\t" // user flags
		    "push $%c2			\n\t" // user cs
		    "push $%c1			\n\t" // user eip
		    "mov  $0x175, %%ecx		\n\t"
		    "rdmsr			\n\t"
		    "mov  %%eax, %%ebp		\n\t" // save sysenter esp
		    "mov  %%esp, %%eax		\n\t"
		    "wrmsr			\n\t" // change sysenter esp
		    "mov  $%c3, %%eax		\n\t"
		    "mov  %%eax, %%ds		\n\t" // ensure dpl3 at ds
		    "mov  %%eax, %%es		\n\t" // ensure dpl3 at es
		    "mov  %%eax, %%fs		\n\t" // ensure dpl3 at fs
		    "mov  %%eax, %%gs		\n\t" // ensure dpl3 at gs

		    "rdtsc			\n\t"
		    "mov  %%eax, %%esi		\n\t"
		    "movl $200001, %%eax	\n\t"

		    ".align 8			\n\t"
		    "1:				\n\t"
		    "dec  %%eax			\n\t"
		    "jz   2f			\n\t"
		    "iret			\n\t"

		    "2:				\n\t"
		    "rdtsc			\n\t"
		    "sub  %%eax, %%esi		\n\t"
		    "sub  %%edx, %%edx		\n\t"
		    "mov  $0x176, %%ecx		\n\t"
		    "mov  %%ebx, %%eax		\n\t"
		    "wrmsr			\n\t" // restore sysenter eip
		    "mov  $0x175, %%ecx		\n\t"
		    "mov  %%ebp, %%eax		\n\t"
		    "wrmsr			\n\t" // restore sysenter esp

		    "mov  %%edi, %%esp		\n\t" // restore esp
		    "pop  %%gs			\n\t"
		    "pop  %%fs			\n\t"
		    "pop  %%es			\n\t"
		    "pop  %%ds			\n\t"
		    "mov  %%esi, %%eax		\n\t"
		    "neg  %%eax			\n\t"
		    "pop  %%ebp			\n\t"
		    "popf			\n\t"
		    : "=A"(time)
		    : "i"(Mem_layout::Tbuf_status_page + Config::PAGE_SIZE-2),
		      "i"(Gdt::gdt_code_user | Gdt::Selector_user),
		      "i"(Gdt::gdt_data_user | Gdt::Selector_user)
		    : "ebx", "ecx", "esi", "edi", "memory");
      show_time(time, 200000, "sysenter + iret");
    }
  if (Cpu::have_syscall())
    {
      // Enable syscall/sysret, set Syscall MSR, write "syscall" to the last
      // two bytes of the Tbuf status page (which is accessible from usermode)
      // and do "sysret ; syscall". Gdt doesn't care us since sysret loads a
      // flat segment. Sysret enables the interrupts again so make sure that
      // we don't receive a timer interrupt.
      Timer_tick::disable(Cpu_number::boot_cpu());
      Proc::sti();
      Proc::irq_chance();
      asm volatile ("pushf			\n\t"
		    "push %%ebp			\n\t"
		    "mov  $0xC0000080, %%ecx	\n\t"
		    "sub  %%edx, %%edx		\n\t"
		    "movl $1, %%eax		\n\t" // enable syscall
		    "wrmsr			\n\t"
		    "mov  $0xC0000081, %%ecx	\n\t"
		    "rdmsr			\n\t"
		    "mov  %%eax, %%ebx		\n\t" // save IA32_STAR
		    "mov  %%edx, %%ebp		\n\t"
		    "mov  $1f, %%eax		\n\t" // syscall eip
		    "mov  $0x00180008, %%edx	\n\t" // syscall cs+ss
		    "wrmsr			\n\t"
		    "mov  %%esp, %%edi		\n\t"
		    "movw $0x050f, %c1		\n\t"
		    "rdtsc			\n\t"
		    "mov  %%eax, %%esi		\n\t"
		    "movl $200001, %%eax	\n\t"
		    "mov  $(%c1+2), %%ecx	\n\t"

		    ".align 8			\n\t"
		    "1:				\n\t"
		    "dec  %%eax			\n\t"
		    "jz   2f			\n\t"
		    "sub  $2, %%ecx		\n\t"
		    "sysret			\n\t"

		    "2:				\n\t"
		    "rdtsc			\n\t"
		    "sub  %%eax, %%esi		\n\t"
		    "mov  $0xC0000081, %%ecx	\n\t"
		    "mov  %%ebx, %%eax		\n\t"
		    "mov  %%ebp, %%edx		\n\t"
		    "wrmsr			\n\t" // restore IA32_STAR

		    "mov  %%esi, %%eax		\n\t"
		    "neg  %%eax			\n\t"
		    "mov  %%edi, %%esp		\n\t"
		    "pop  %%ebp			\n\t"
		    "sub  %%edx, %%edx		\n\t"
		    "popf			\n\t"
		    : "=A"(time)
		    : "i"(Mem_layout::Tbuf_status_page + Config::PAGE_SIZE-2)
		    : "ebx", "ecx", "esi", "edi", "memory");
      Proc::cli();
      Timer_tick::enable(Cpu_number::boot_cpu());
      show_time(time, 200000, "syscall + sysret");
    }
  BENCH("push EAX + pop EAX",	inst_push_pop,     200000);
  BENCH("push flags + pop EAX", inst_pushf_pop,    200000);
    {
      time = Cpu::rdtsc();
      asm volatile ("mov $200000,%%ebx\n\t"
		    ".align 4; 1: rdtsc; dec %%ebx; jnz 1b"
		    : : : "eax", "ebx", "edx");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "rdtsc");
    }
  if (Cpu::boot_cpu()->local_features() & Cpu::Lf_rdpmc)
    {
      time = Cpu::rdtsc();
      asm volatile ("mov $200000,%%ebx; movl $0x00000000,%%ecx\n\t"
		    ".align 4; 1: rdpmc; dec %%ebx; jnz 1b"
		    : : : "eax", "ebx", "ecx", "edx");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "rdpmc(0)");
    }
  if (Cpu::boot_cpu()->local_features() & Cpu::Lf_rdpmc32)
    {
      time = Cpu::rdtsc();
      asm volatile ("mov $200000,%%ebx; movl $0x80000000,%%ecx\n\t"
		    ".align 4; 1: rdpmc; dec %%ebx; jnz 1b"
		    : : : "eax", "ebx", "ecx", "edx");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "rdpmc32(0)");
    }
  if (Config::apic)
    {
      BENCH("APIC timer read", inst_apic_timer_read, 200000);
    }
    {
      time = Cpu::rdtsc();
      for (i=200000; i; i--)
	asm volatile ("invlpg %c2	\n\t"
		      "mov %c2, %1	\n\t"
		      : "=r" (dummy), "=r" (dummy)
		      : "i"(Mem_layout::Jdb_bench_page));
      time = Cpu::rdtsc() - time - time_invlpg;
      show_time (time, 200000, "load data TLB (4k)");
    }
    {
      // asm ("1: mov %%cr3,%%edx; mov %%edx, %%cr3; dec %%eax; jnz 1b; ret")
      *(Unsigned32*)(Mem_layout::Jdb_bench_page + 0xff0) = 0x0fda200f;
      *(Unsigned32*)(Mem_layout::Jdb_bench_page + 0xff4) = 0x7548da22;
      *(Unsigned32*)(Mem_layout::Jdb_bench_page + 0xff8) = 0xc3f7;

      Mem::barrier();
      time = Cpu::rdtsc();
      asm volatile ("call  *%%ecx"
		    : "=a"(dummy)
		    : "c"(Mem_layout::Jdb_bench_page + 0xff0), "a"(200000)
		    : "edx");

      time = Cpu::rdtsc() - time - time_reload_cr3;
      show_time (time, 200000, "load code TLB (4k)");
    }

  Proc::sti_restore(flags);
}
