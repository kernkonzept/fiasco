IMPLEMENTATION:

#include <cstdio>
#include "cpu.h"
#include "config_gdt.h"
#include "div32.h"
#include "gdt.h"
#include "kmem_mmio.h"
#include "simpleio.h"
#include "static_init.h"
#include "timer.h"

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
		: : "m" (*reinterpret_cast<char*>(Mem_layout::Jdb_tmp_map_page)))

#define inst_read_cr3							\
  asm volatile ("mov %%cr3,%0"						\
		: "=r"(dummy))

#define inst_reload_cr3							\
  asm volatile ("mov %%cr3,%0; mov %0,%%cr3"				\
		: "=r"(dummy))

#define inst_reload_cr3_no_flush					\
  asm volatile ("mov %%cr3,%0; bts $63, %0; mov %0,%%cr3"		\
		: "=r"(dummy))

#define inst_clts							\
  asm volatile ("clts")

#define inst_cli_sti							\
  asm volatile ("cli; sti")

#define inst_set_cr0_ts							\
  asm volatile ("mov %%cr0,%0; or %1,%0; mov %0,%%cr0"		\
		: "=r" (dummy) : "i" (CR0_TS))

#define inst_push_pop							\
  asm volatile ("push %rax; pop %rax")

#define inst_pushf_pop							\
  asm volatile ("pushf; pop %%rax" : : : "rax")

#define inst_in8_pic							\
  asm volatile ("inb $0x21, %%al" : "=a" (dummy))

#define inst_in8_80							\
  asm volatile ("inb $0x80, %%al" : "=a" (dummy))

#define inst_out8_pic							\
  asm volatile ("outb %%al, $0x21" : : "a" (0xff))

#define inst_apic_timer_read						\
  asm volatile ("" : : "r"(Apic::timer_reg_read()))

asm (".p2align 1                                        \n\t"
     "bench_verify_selector:                            \n\t"
     ".word " FIASCO_STRINGIFY(GDT_DATA_KERNEL) "       \n\t");

#define inst_verr_reg                                                   \
  asm volatile ("mov $" FIASCO_STRINGIFY(GDT_DATA_KERNEL) ", %%r13 \n\t" \
                "verr %%r13w                                    \n\t"   \
                : : : "r13");

#define inst_verw_reg                                                   \
  asm volatile ("mov $" FIASCO_STRINGIFY(GDT_DATA_KERNEL) ", %%r13 \n\t" \
                "verw %%r13w                                    \n\t"   \
                : : : "r13");

#define inst_verr_mem                                                   \
  asm volatile ("verr bench_verify_selector");

#define inst_verw_mem                                                   \
  asm volatile ("verw bench_verify_selector");

#define BENCH(name, instruction, rounds)				\
  do									\
    {									\
      time = Cpu::rdtsc();						\
      for (i=rounds; i; i--)						\
        instruction;							\
      time = Cpu::rdtsc() - time;					\
      show_time (time, rounds, name);					\
    } while (0)

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

  // need cached, non-global mapping for measuring the time to load TLB entries
  Address phys =
    Kmem::virt_to_phys(reinterpret_cast<void*>(Mem_layout::Tbuf_status_page));
  void *bench_page = Kmem_mmio::map(phys, Config::PAGE_SIZE, true, true, false);
  if (!bench_page)
    {
      printf("Couldn't map benchmark page!\n");
      return;
    }

    {
      time = Cpu::rdtsc();
      asm volatile ("mov $10000000,%%rcx; .align 4;\n\t"
		    "1:dec %%rcx; jnz 1b" : : : "rcx");
	
      time = Cpu::rdtsc() - time;
      show_time(time, 10000000, "1:dec RCX, jnz 1b");
    }
  BENCH("wbinvd",		inst_wbinvd,         5000);
  BENCH("invlpg",		inst_invlpg,       200000);
  time_invlpg = time;
  BENCH("read CR3",		inst_read_cr3,     200000);
  BENCH("reload CR3",		inst_reload_cr3,   200000);
  time_reload_cr3 = time;
  if constexpr (Config::Pcid_enabled)
    BENCH("reload CR3/nf",	inst_reload_cr3_no_flush, 200000);
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
      asm volatile ("mov $200000,%%rbx\n\t"
		    ".align 8; 1: mov %%es, %%eax; dec %%rbx; jnz 1b"
		    : : : "rax", "rbx");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "get ES");
    }
    {
      // set ES segment descriptor and access memory through the ES segment
      time = Cpu::rdtsc();
      asm volatile ("mov  $200000,%%rbx		\n\t"
		    ".align 8			\n\t"
		    "1:				\n\t"
		    "mov  %%rax, %%es		\n\t"
		    "dec  %%rbx			\n\t"
		    "mov  %%es:(%c1),%%rdi	\n\t"
		    "jnz  1b			\n\t"
		    :
		    : "a"(Gdt::gdt_data_kernel | Gdt::Selector_kernel),
		      "i"(Mem_layout::Kernel_image)
		    : "rbx", "rdi");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "set ES + load ES:mem");
    }
    {
      // write a GDT entry, set ES segment descriptor to this gdt entry
      // and access memory through the ES segment
      Unsigned32 *gdt_e =
        reinterpret_cast<Unsigned32*>(&(*gdt)[Gdt::gdt_data_kernel/8]);
      time = Cpu::rdtsc();
      asm volatile ("mov  $200000,%%rbx		\n\t"
		    ".align 16			\n\t"
		    "1:				\n\t"
		    "mov  %%rax, (%%rsi)	\n\t"
		    "mov  %%rcx, 4(%%rsi)	\n\t"
		    "mov  %%rdx, %%es		\n\t"
		    "dec  %%rbx			\n\t"
		    "mov  %%es:(%c4),%%rdi	\n\t"
		    "jnz  1b			\n\t"
		    :
		    : "a"(gdt_e[0]), "c"(gdt_e[1]),
		      "d"(Gdt::gdt_data_kernel | Gdt::Selector_kernel),
		      "S"(gdt->entries() + Gdt::gdt_data_kernel/8),
		      "i"(Mem_layout::Kernel_image)
		    : "rbx", "rdi");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "modify ES + load ES:mem");
    }
  BENCH("verr reg",             inst_verr_reg,     200000);
  BENCH("verw reg",             inst_verw_reg,     200000);
  BENCH("verr mem",             inst_verr_mem,     200000);
  BENCH("verw mem",             inst_verw_mem,     200000);
  BENCH("push RAX + pop RAX",	inst_push_pop,     200000);
  BENCH("push flags + pop RAX", inst_pushf_pop,    200000);
    {
      time = Cpu::rdtsc();
      asm volatile ("mov $200000,%%rbx\n\t"
		    ".align 4; 1: rdtsc; dec %%rbx; jnz 1b"
		    : : : "rax", "rbx", "rdx");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "rdtsc");
    }
  if (Cpu::boot_cpu()->local_features() & Cpu::Lf_rdpmc)
    {
      time = Cpu::rdtsc();
      asm volatile ("mov $200000,%%rbx; mov $0x00000000,%%rcx\n\t"
		    ".align 4; 1: rdpmc; dec %%rbx; jnz 1b"
		    : : : "rax", "rbx", "rcx", "rdx");
      time = Cpu::rdtsc() - time;
      show_time(time, 200000, "rdpmc(0)");
    }
  if (Cpu::boot_cpu()->local_features() & Cpu::Lf_rdpmc32)
    {
      time = Cpu::rdtsc();
      asm volatile ("mov $200000,%%rbx; mov $0x80000000,%%rcx\n\t"
		    ".align 4; 1: rdpmc; dec %%rbx; jnz 1b"
		    : : : "rax", "rbx", "rcx", "rdx");
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
	asm volatile ("invlpg %1	\n\t"
		      "mov %1, %0	\n\t"
		      : "=r" (dummy)
		      : "m" (*static_cast<char*>(bench_page)));
      time = Cpu::rdtsc() - time - time_invlpg;
      show_time (time, 200000, "load data TLB (4k)");
    }

    {
      // asm ("1: mov %%cr3,%%rdx; mov %%rdx, %%cr3; dec %%rax; jnz 1b; ret")
      Unsigned32 *words = static_cast<Unsigned32*>(bench_page);
      words[0xff0/4] = 0x0fda200f;
      words[0xff4/4] = 0xff48da22;
      words[0xff8/4] = 0xc3f575c8;

      Mem::barrier();
      time = Cpu::rdtsc();
      asm volatile ("call  *%%rcx"
                    : "=a"(dummy)
                    : "c"(reinterpret_cast<Mword>(bench_page) + 0xff0),
                      "a"(200000)
                    : "rdx");

      time = Cpu::rdtsc() - time - time_reload_cr3;
      show_time (time, 200000, "load code TLB (4k)");
    }

  Kmem_mmio::unmap(bench_page, Config::PAGE_SIZE);

  Proc::sti_restore(flags);
}
