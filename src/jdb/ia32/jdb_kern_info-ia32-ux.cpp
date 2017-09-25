IMPLEMENTATION[ia32,ux,amd64]:

#include <cstdio>
#include <cstring>
#include "simpleio.h"
#include "jdb_screen.h"

PUBLIC
void
Jdb_kern_info_cpu::show_f_bits(unsigned features, const char *const *table,
                               unsigned first_pos, unsigned &last_pos,
                               unsigned &colon)
{
  unsigned i, count;

  for (i = count = 0; *table != (char *)-1; i++, table++)
    if ((features & (1 << i)) && *table)
      {
	int slen = strlen(*table);
	if (last_pos+colon + slen > 78)
	  {
	    colon = 0;
	    last_pos = first_pos;
	    printf("\n%*s", (int)first_pos, "");
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
    (char *)(-1)
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
    NULL, NULL, NULL, NULL,
    "sse41", "sse42",
    NULL, NULL,
    "popcnt", NULL,
    "aes", "xsave", "osxsave",
    "avx", "f16c",
    (char *)(-1)
  };
  static const char *const ext_81_ecx[] =
  {
    NULL, NULL, "svm (secure virtual machine)", NULL, NULL,
    "abm (adv bit manipulation)", "SSE4A", NULL,
    NULL, "OSVW (OS visible workaround)", NULL, NULL,
    "SKINIT", "WDT (watchdog timer support)", NULL,
    "lwp", "fmaa", NULL, NULL, "nodeid", NULL, "tbm", "topext",
    (char *)(-1)
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
    (char *)(-1)
  };

  unsigned position = 5, colon = 0;
  putstr("CPU features:\n     ");
  show_f_bits (Cpu::boot_cpu()->features(), simple, 5, position, colon);
  show_f_bits (Cpu::boot_cpu()->ext_features(), extended, 5, position, colon);
  show_f_bits (Cpu::boot_cpu()->ext_8000_0001_ecx(), ext_81_ecx, 5, position, colon);
  show_f_bits (Cpu::boot_cpu()->ext_8000_0001_edx(), ext_81_edx, 5, position, colon);
}

PRIVATE inline NEEDS["jdb_screen.h"]
void
Jdb_kern_info_misc::show_pdir()
{
  Mem_space *s = Mem_space::current_mem_space(Cpu_number::boot_cpu());
  printf("%s" L4_PTR_FMT "\n",
         Jdb_screen::Root_page_table, (Address)s->dir());
}
