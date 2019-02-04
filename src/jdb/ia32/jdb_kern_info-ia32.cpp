IMPLEMENTATION[ia32 || amd64]:

#include <cstdio>
#include <cstring>
#include "minmax.h"
#include "simpleio.h"

#include "config.h"
#include "cpu.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "i8259.h"
#include "msrdefs.h"
#include "jdb_screen.h"
#include "perf_cnt.h"
#include "pic.h"
#include "space.h"
#include "tss.h"


class Jdb_kern_info_idt : public Jdb_kern_info_module
{
};

static Jdb_kern_info_idt k_I INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);

PUBLIC
Jdb_kern_info_idt::Jdb_kern_info_idt()
  : Jdb_kern_info_module('I', "Interrupt Descriptor Table (IDT)")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_idt::show() override
{
  Pseudo_descriptor idt_pseudo;
  unsigned line = 0;

  Idt::get(&idt_pseudo);
  unsigned idt_max_bytes = idt_pseudo.limit() + 1;
  unsigned idt_max_entries = idt_max_bytes / sizeof(Idt_entry);

  printf("IDT base=" L4_PTR_FMT "  limit=%04x (%d entries)\n",
         idt_pseudo.base(), idt_max_bytes, idt_max_entries);
  if (!Jdb_core::new_line(line))
    return;

  Idt_entry *ie = reinterpret_cast<Idt_entry*>(idt_pseudo.base());
  // On VM exit, IDTR (and GDTR) limit are each set to 0xffff. We don't bother
  // because IDT entries beyond 256 are ignored. Show only up to 256 entries.
  for (unsigned i = 0; i < min(Idt::_idt_max, idt_max_entries); ++i)
    {
      printf("%3x: ",i);
      ie[i].show();
      if (!Jdb_core::new_line(line))
        return;
    }
}

class Jdb_kern_info_test_tsc_scaler : public Jdb_kern_info_module
{
};

static Jdb_kern_info_test_tsc_scaler k_tts INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_test_tsc_scaler::Jdb_kern_info_test_tsc_scaler()
  : Jdb_kern_info_module('T', "Test TSC scaler")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_test_tsc_scaler::show() override
{
  while (Kconsole::console()->getchar(false) == -1)
    {
      Unsigned64 t;
      t = Cpu::boot_cpu()->ns_to_tsc(Cpu::boot_cpu()->tsc_to_ns(Cpu::rdtsc()));
      printf("Diff (press any key to stop): %llu\n", Cpu::rdtsc() - t);
    }
}

class Jdb_kern_info_pic_state : public Jdb_kern_info_module
{
};

static Jdb_kern_info_pic_state k_p INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_pic_state::Jdb_kern_info_pic_state()
  : Jdb_kern_info_module('p', "PIC ports")
{
  Jdb_kern_info::register_subcmd(this);
}

void
Jdb_kern_info_pic_state::show() override
{
  typedef Irq_chip_i8259<Io> I8259;
  int i;
  static char const hex[] = "0123456789ABCDEF";

  // show important I/O ports
  Io::out8_p(I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IS_ONRD,
             Pic::MASTER_ICW );
  unsigned in_service = Io::in8(Pic::MASTER_ICW);
  Io::out8_p(I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IR_ONRD,
             Pic::MASTER_ICW);
  unsigned requested = Io::in8(Pic::MASTER_ICW);
  unsigned mask = Jdb::pic_status & 0x0ff;
  printf("master PIC: in service:");
  for (i=7; i>=0; i--)
    putchar((in_service & (1<<i)) ? hex[i] : '-');
  printf(", request:");
  for (i=7; i>=0; i--)
    putchar((requested & (1<<i)) ? hex[i] : '-');
  printf(", mask:");
  for (i=7; i>=0; i--)
    putchar((mask & (1<<i)) ? hex[i] : '-');
  putchar('\n');

  Io::out8_p( I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IS_ONRD,
              Pic::SLAVES_ICW);
  in_service = Io::in8(Pic::SLAVES_ICW);
  Io::out8_p( I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IR_ONRD,
              Pic::SLAVES_ICW);
  requested = Io::in8(Pic::SLAVES_ICW);
  mask = Jdb::pic_status >> 8;
  printf(" slave PIC: in service:");
  for (i=7; i>=0; i--)
    putchar((in_service & (1<<i)) ? hex[i+8] : '-');
  printf(", request:");
  for (i=7; i>=0; i--)
    putchar((requested & (1<<i)) ? hex[i+8] : '-');
  printf(", mask:");
  for (i=7; i>=0; i--)
    putchar((mask & (1<<i)) ? hex[i+8] : '-');
  putchar('\n');
}


class Jdb_kern_info_misc : public Jdb_kern_info_module
{
};

static Jdb_kern_info_misc k_i INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);

PUBLIC
Jdb_kern_info_misc::Jdb_kern_info_misc()
  : Jdb_kern_info_module('i', "Miscellaneous info")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_misc::show() override
{
  Cpu_time clock = Jdb::system_clock_on_enter();
  printf("clck: %08llx.%08llx\n", clock >> 32, clock & 0xffffffffU);

  show_pdir();

  Pseudo_descriptor gdt_pseudo, idt_pseudo;
  Gdt::get (&gdt_pseudo);
  Idt::get (&idt_pseudo);
  printf("idt : base=" L4_PTR_FMT "  limit=%04x\n"
         "gdt : base=" L4_PTR_FMT "  limit=%04x\n",
         idt_pseudo.base(), (idt_pseudo.limit() + 1) / 8,
         gdt_pseudo.base(), (gdt_pseudo.limit() + 1) / 8);

  // print LDT
  printf("ldt : %04x", Cpu::get_ldt());
  if (Cpu::get_ldt() != 0)
    {
      Gdt_entry *e = Cpu::boot_cpu()->get_gdt()->entries()
                     + (Cpu::boot_cpu()->get_ldt() >> 3);
      printf(": " L4_PTR_FMT "-" L4_PTR_FMT,
             e->base(), e->base()+ e->size());
    }

  // print TSS
  printf("\n"
         "tr  : %04x", Cpu::boot_cpu()->get_tr());

  if (Cpu::get_tr() != 0)
    {
      Gdt_entry *e = Cpu::boot_cpu()->get_gdt()->entries()
                     + (Cpu::boot_cpu()->get_tr() >> 3);
      printf(": " L4_PTR_FMT "-" L4_PTR_FMT ", iobitmap at " L4_PTR_FMT,
             e->base(), e->base() + e->size(),
             e->base() + (reinterpret_cast<Tss *>(e->base())->_hw.ctx.iopb));
    }

  printf("\n"
         "cr0 : " L4_PTR_FMT "\n"
         "cr4 : " L4_PTR_FMT "\n",
         Cpu::get_cr0(), Cpu::get_cr4());
}

PRIVATE inline NEEDS["jdb_screen.h"]
void
Jdb_kern_info_misc::show_pdir()
{
  Mem_space *s = Mem_space::current_mem_space(Cpu_number::boot_cpu());
  printf("%s" L4_PTR_FMT "\n",
         Jdb_screen::Root_page_table, reinterpret_cast<Address>(s->dir()));
}


class Jdb_kern_info_cpu : public Jdb_kern_info_module
{
};

static Jdb_kern_info_cpu k_c INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);

PUBLIC
Jdb_kern_info_cpu::Jdb_kern_info_cpu()
  : Jdb_kern_info_module('c', "CPU features")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_cpu::show() override
{
  const char *perf_type = Perf_cnt::perf_type();
  char cpu_mhz[32];
  char time[32];
  unsigned hz;
  constexpr char const * const scheduler_mode[]
    = { "PIT", "RTC", "APIC", "HPET" };

  cpu_mhz[0] = '\0';
  if ((hz = Cpu::boot_cpu()->frequency()))
    {
      unsigned mhz = hz / 1000000;
      hz -= mhz * 1000000;
      unsigned khz = hz / 1000;
      snprintf(cpu_mhz, sizeof(cpu_mhz), "%u.%03u MHz", mhz, khz);
    }

  printf ("CPU: %s %s (%s)\n",
          Cpu::boot_cpu()->model_str(), cpu_mhz,
          Config::found_vmware ? "vmware" : "native");
  Cpu::boot_cpu()->show_cache_tlb_info("     ");
  show_features();
  show_feature_ia32_tsc_adjust();

  if (Cpu::boot_cpu()->tsc())
    {
      Unsigned32 hour, min, sec, ns;
      Cpu::boot_cpu()->tsc_to_s_and_ns(Cpu::rdtsc(), &sec, &ns);
      hour = sec  / 3600;
      sec -= hour * 3600;
      min  = sec  / 60;
      sec -= min  * 60;
      snprintf(time, sizeof(time), "%02u:%02u:%02u.%06u",
               hour, min, sec, ns/1000);
    }
  else
    strcpy(time, "not available");

  printf("\nTimer interrupt source: %s (irq vector 0x%02x)"
         "\nPerformance counters: %s"
         "\nLast branch recording: %s"
         "\nDebug store to memory: %s"
         "\nTime stamp counter: %s"
         "\n",
         scheduler_mode[Config::Scheduler_mode],
         Config::scheduler_irq_vector,
         perf_type ? perf_type : "no",
         Cpu::boot_cpu()->lbr_type() != Cpu::Lbr_unsupported
            ? Cpu::boot_cpu()->lbr_type() == Cpu::Lbr_pentium_4 ? "P4" : "P6"
            : "no",
         Cpu::boot_cpu()->bts_type() != Cpu::Bts_unsupported
            ? Cpu::boot_cpu()->bts_type() == Cpu::Bts_pentium_4 ? "P4" : "Pentium-M"
            : "no",
         time
         );
}

PRIVATE
void
Jdb_kern_info_cpu::show_feature_ia32_tsc_adjust()
{
  if (Cpu::cpuid_eax(0) >= 7 && (Cpu::cpuid_ebx(7) & 2))
    {
      printf("\nIA32_TSC_ADJUST values:\n");
      for (Cpu_number u = Cpu_number::first(); u < Config::max_num_cpus(); ++u)
        if (Cpu::online(u))
          {
            printf("     CPU[%2u]: ", cxx::int_value<Cpu_number>(u));

            auto get_tsc_adj = [](Cpu_number)
              {
                Unsigned64 v = ~0ULL;
                Jdb::rdmsr(Msr_ia32_tsc_adjust, &v);
                printf("%lld\n", v);
              };

            if (u == Cpu_number::boot_cpu())
              get_tsc_adj(u);
            else
              Jdb::remote_work(u, get_tsc_adj, true);

            Mem::barrier();
          }
    }
}

static char const *end_of_table = reinterpret_cast<char const *>(-1);

PUBLIC
void
Jdb_kern_info_cpu::show_f_bits(unsigned features, const char *const *table,
                               unsigned first_pos, unsigned &last_pos,
                               unsigned &colon)
{
  unsigned i, count;

  for (i = count = 0; *table != end_of_table; i++, table++)
    if ((features & (1 << i)) && *table)
      {
	int slen = strlen(*table);
	if (last_pos+colon + slen > 78)
	  {
	    colon = 0;
	    last_pos = first_pos;
	    printf("\n%*s", first_pos, "");
	  }
	printf ("%s%s", colon ? ", " : "", *table);
	last_pos += slen + colon;
	colon = 2;
      }
}

PUBLIC
void
Jdb_kern_info_cpu::show_features()
{
  static const char *const simple[] =
  {
    "fpu (fpu on chip)",
    "vme (virtual-8086 mode enhancements)",
    "de (I/O breakpoints)",
    "pse (4MB pages)",
    "tsc (rdtsc instruction)",
    "msr (rdmsr/rdwsr instructions)",
    "pae (physical address extension)",
    "mce (machine check exception #18)",
    "cx8 (cmpxchg8 instruction)",
    "apic (on-chip APIC)",
    NULL,
    "sep (sysenter/sysexit instructions)",
    "mtrr (memory type range registers)",
    "pge (global TLBs)",
    "mca (machine check architecture)",
    "cmov (conditional move instructions)",
    "pat (page attribute table)",
    "pse36 (32-bit page size extension)",
    "psn (processor serial number)",
    "clfsh (flush cache line instruction)",
    NULL,
    "ds (debug store to memory)",
    "acpi (thermal monitor and soft controlled clock)",
    "mmx (MMX technology)",
    "fxsr (fxsave/fxrstor instructions)",
    "sse (SSE extensions)",
    "sse2 (SSE2 extensions)",
    "ss (self snoop of own cache structures)",
    "htt (hyper-threading technology)",
    "tm (thermal monitor)",
    NULL,
    "pbe (pending break enable)",
    end_of_table
  };

  static const char *const extended[] =
  {
    "pni (prescott new instructions)",
    NULL, NULL,
    "monitor (monitor/mwait instructions)",
    "dscpl (CPL qualified debug store)",
    "vmx (virtual machine technology)",
    NULL,
    "est (enhanced speedstep technology)",
    "tm2 (thermal monitor 2)",
    NULL,
    "cid (L1 context id)",
    NULL, NULL,
    "cmpxchg16b",
    "xtpr (send task priority messages)",
    NULL, NULL,
    "pcid",
    NULL,
    "sse41", "sse42",
    "x2apic",
    NULL,
    "popcnt", NULL,
    "aes", "xsave", "osxsave",
    "avx", "f16c",
    end_of_table
  };

  static const char *const ext_81_ecx[] =
  {
    NULL, NULL, "svm (secure virtual machine)", NULL, NULL,
    "abm (adv bit manipulation)", "SSE4A", NULL,
    NULL, "OSVW (OS visible workaround)", NULL, NULL,
    "SKINIT", "WDT (watchdog timer support)", NULL,
    "lwp", "fmaa", NULL, NULL, "nodeid", NULL, "tbm", "topext",
    end_of_table
  };

  static const char *const ext_81_edx[] =
  {
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL,
    "syscall (syscall/sysret instructions)",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL,
    "mp (MP capable)",
    "nx (no-execute page protection)",
    NULL,
    "mmxext (AMD extensions to MMX)",
    NULL, NULL,
    "fxsr_opt (FXSR optimizations)",
    "Page1GB",
    "RDTSCP",
    NULL, // reserved
    "lm (Long mode)",
    "3dnowext (AMD 3DNow! extenstion)",
    "3dnow (3DNow! instructions)",
    end_of_table
  };

  unsigned position = 5, colon = 0;
  putstr("\nCPU features:\n     ");
  show_f_bits (Cpu::boot_cpu()->features(), simple, 5, position, colon);
  show_f_bits (Cpu::boot_cpu()->ext_features(), extended, 5, position, colon);
  show_f_bits (Cpu::boot_cpu()->ext_8000_0001_ecx(), ext_81_ecx, 5, position, colon);
  show_f_bits (Cpu::boot_cpu()->ext_8000_0001_edx(), ext_81_edx, 5, position, colon);

  puts("\n\nRaw CPUID features:");
  // below we use arbitrary upper limits for basic/extended leaf
  Unsigned32 max = min(0x2fU, Cpu::cpuid_eax(0));
  for (Unsigned32 i = 0; i <= max; ++i)
    {
      Unsigned32 eax, ebx, ecx, edx;
      Cpu::cpuid(i, 0, &eax, &ebx, &ecx, &edx);
      printf("     %08x: %08x %08x %08x %08x\n", i, eax, ebx, ecx, edx);
      if (i == max && max < 0x80000000U)
        {
          i = 0x80000000 - 1;
          max = min(0x8000001fU, Cpu::cpuid_eax(i + 1));
          putchar('\n');
        }
    }
}


class Jdb_kern_info_gdt : public Jdb_kern_info_module
{
private:
  static unsigned line;
};

static Jdb_kern_info_gdt k_g INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);

unsigned Jdb_kern_info_gdt::line;

PUBLIC
Jdb_kern_info_gdt::Jdb_kern_info_gdt()
  : Jdb_kern_info_module('g', "Global Descriptor Table (GDT)")
{
  Jdb_kern_info::register_subcmd(this);
}

PRIVATE static
void
Jdb_kern_info_gdt::show_gdt(Cpu_number cpu)
{
  Gdt *gdt = Cpu::cpus.cpu(cpu).get_gdt();
  unsigned entries = Gdt::gdt_max / 8;

  if constexpr (Config::Max_num_cpus > 1)
    printf("CPU%u: GDT base=" L4_PTR_FMT "  limit=%04x (%04x bytes)\n",
           cxx::int_value<Cpu_number>(cpu), reinterpret_cast<Mword>(gdt),
           entries, Gdt::gdt_max);
  else
    printf("GDT base=" L4_PTR_FMT "  limit=%04x (%04x bytes)\n",
           reinterpret_cast<Mword>(gdt), entries, Gdt::gdt_max);

  if (!Jdb_core::new_line(line))
    return;

  for (unsigned i = 0; i < entries; i++)
    {
      printf(" %02x: ", i * 8);
      if (i == 0)
        printf("(ignored)\n");
      else
        (*gdt)[i].show();
      if (!Jdb_core::new_line(line))
        return;
      if (i != 0 && (*gdt)[i].desc_size() == 16)
        ++i;
    }
}

PUBLIC
void
Jdb_kern_info_gdt::show() override
{
  line = 0;
  Jdb::foreach_cpu(&show_gdt);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && hpet_timer]:

#include "hpet.h"

class Jdb_kern_info_hpet_smm : public Jdb_kern_info_module
{};

static Jdb_kern_info_hpet_smm ki_smm INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);

PUBLIC
Jdb_kern_info_hpet_smm::Jdb_kern_info_hpet_smm()
  : Jdb_kern_info_module('S', "SMM loop using HPET")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_hpet_smm::show() override
{
  const unsigned config_spin_loops = 10000;
  const unsigned config_hist_loops = 60;
  unsigned delta = 1;
  Mword counter_good = 0;
  Mword histsum = 0;
  Mword hist_loops = config_hist_loops;

  printf("HPET SMM Check: Press key to stop.\n");
  printf("HPET SMM Check Loop testing (loops=%d)\n", config_spin_loops);

  Hpet::hpet()->dump();
  Hpet::hpet()->enable();
  while (1)
    {
      Unsigned64 x1 = Hpet::hpet()->counter_val;

      int i = config_spin_loops;
      while (i--)
        asm volatile("" : : : "memory");

      Unsigned64 diff = Hpet::hpet()->counter_val - x1;

      if (hist_loops)
        {
          histsum += diff;
          --hist_loops;

          if (hist_loops == 0)
            {
              delta = (histsum + histsum / 9) / config_hist_loops;
              printf("HPET SMM Check threshold=%dhpet-clks %lldus\n",
                     delta,
                     (delta * Hpet::hpet()->counter_clk_period()) / 1000000000ULL);
            }
        }
      else
        {
          if (diff > delta && diff < (~0UL - delta * 2))
            {
              printf("%lld  %lldus (before %ld good iterations)\n", diff,
                     (diff * Hpet::hpet()->counter_clk_period()) / 1000000000ULL,
                     counter_good);
              counter_good = 0;
              if (Kconsole::console()->getchar(false) != -1)
                break;
            }
          else
            ++counter_good;

          if (counter_good % 30000 == 2)
            if (Kconsole::console()->getchar(false) != -1)
              break;
        }
    }
}
