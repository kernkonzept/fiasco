IMPLEMENTATION[ia32 || amd64]:

#include <cstdio>
#include "config.h"
#include "cpu.h"
#include "jdb.h"
#include "jdb_ktrace.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "static_init.h"
#include "task.h"
#include "x86desc.h"

class Jdb_misc_general : public Jdb_module
{
public:
  Jdb_misc_general() FIASCO_INIT;
private:
  static char first_char;
};

char Jdb_misc_general::first_char;


PUBLIC
Jdb_module::Action_code
Jdb_misc_general::action(int cmd, void *&, char const *&, int &) override
{
  switch (cmd)
    {
    case 0:
      // escape key
      if (first_char == '+' || first_char == '-')
	{
	  putchar(first_char);
	  Config::esc_hack = (first_char == '+');
	  putchar('\n');
	  return NOTHING;
	}
      return ERROR;
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_misc_general::cmds() const override
{
  static Cmd cs[] =
    {
	{ 0, "E", "esckey", "%C",
	  "E{+|-}\ton/off enter jdb by pressing <ESC>",
	  &first_char },
    };
  return cs;
}

PUBLIC
int
Jdb_misc_general::num_cmds() const override
{
  return 1;
}

IMPLEMENT
Jdb_misc_general::Jdb_misc_general()
  : Jdb_module("GENERAL")
{
}

static Jdb_misc_general jdb_misc_general INIT_PRIORITY(JDB_MODULE_INIT_PRIO);


//---------------------------------------------------------------------------//

class Jdb_misc_debug : public Jdb_module
{
public:
  Jdb_misc_debug() FIASCO_INIT;
private:
  static char     first_char;
  static Mword    task;
};

char     Jdb_misc_debug::first_char;
Mword    Jdb_misc_debug::task;

static void
Jdb_misc_debug::show_lbr_entry(const char *str, Address addr)
{
  printf("%s " L4_PTR_FMT " ", str, addr);
}

PUBLIC
Jdb_module::Action_code
Jdb_misc_debug::action(int cmd, void *&args, char const *&fmt, int &) override
{
  switch (cmd)
    {
    case 0:
      // single step
      if (first_char == '+' || first_char == '-')
	{
	  putchar(first_char);
	  Jdb::set_single_step(Jdb::current_cpu, first_char == '+');
	  putchar('\n');
	}
      break;
    case 1:
      // ldt
      if (args == &task)
	{
	  show_ldt();
	  putchar('\n');
	  return NOTHING;
	}

      // lbr/ldt
      if (first_char == '+' || first_char == '-')
	{
	  Cpu::boot_cpu()->lbr_enable(first_char == '+');
	  putchar(first_char);
	  putchar('\n');
	}
      else if (first_char == 'd')
	{
	  printf("d task=");
	  fmt   = "%q";
	  args  = &task;
	  return EXTRA_INPUT;
	}
      else
	{
	  if (Cpu::boot_cpu()->lbr_type() == Cpu::Lbr_pentium_4 || 
	      Cpu::boot_cpu()->lbr_type() == Cpu::Lbr_pentium_4_ext)
	    {
              Unsigned64 msr_from, msr_to;
              if (   Jdb::rdmsr(MSR_LER_FROM_LIP, &msr_from)
                  && Jdb::rdmsr(MSR_LER_TO_LIP, &msr_to))
                {
                  show_lbr_entry("\nbefore exc:", msr_from);
                  show_lbr_entry(" =>", msr_to);
                }

              Unsigned64 branch_tos;
              if (Jdb::rdmsr(MSR_LASTBRANCH_TOS, &branch_tos))
                {
                  branch_tos &= 0xffffffffU;
                  if (Cpu::boot_cpu()->lbr_type() == Cpu::Lbr_pentium_4)
                    {
                      // older P4 models provide a stack of 4 MSRs
                      for (int i=0, j=branch_tos & 3; i<4; i++)
                        {
                          j = (j+1) & 3;
                          Unsigned64 msr;
                          if (Jdb::rdmsr(MSR_LASTBRANCH_0+j, &msr))
                            {
                              show_lbr_entry("\nbranch/exc:", msr >> 32);
                              show_lbr_entry(" =>", msr);
                            }
                        }
                    }
                  else
                    {
                      // newer P4 models provide a stack of 16 MSR pairs
                      for (int i=0, j=branch_tos & 15; i<16; i++)
                        {
                          j = (j+1) & 15;
                          Unsigned64 msr_from, msr_to;
                          if (Jdb::rdmsr(0x680+j, &msr_from)
                              && Jdb::rdmsr(0x6c0+j, &msr_to))
                            {
                              show_lbr_entry("\nbranch/exc:", msr_from);
                              show_lbr_entry(" =>", msr_to);
                            }
                        }
                    }
                }
	    }
	  else if (Cpu::boot_cpu()->lbr_type() == Cpu::Lbr_pentium_6)
	    {
	      Unsigned64 msr_from_ip, msr_to_ip;
              Unsigned64 msr_from_int, msr_to_int;

              if (   Jdb::rdmsr(MSR_LASTBRANCHFROMIP, &msr_from_ip)
                  && Jdb::rdmsr(MSR_LASTBRANCHTOIP, &msr_to_ip)
                  && Jdb::rdmsr(MSR_LASTINTFROMIP, &msr_from_int)
                  && Jdb::rdmsr(MSR_LASTINTTOIP, &msr_to_int))
                {
                  show_lbr_entry("\nbranch:", msr_from_ip);
                  show_lbr_entry(" =>", msr_to_ip);
                  show_lbr_entry("\n   int:", msr_from_int);
                  show_lbr_entry(" =>", msr_to_int);
                }
	    }
	  else
	    printf("Last branch recording feature not available");

	  putchar('\n');
	  break;
	}
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_misc_debug::cmds() const override
{
  static Cmd cs[] =
    {
	{ 0, "S", "singlestep", "%C",
	  "S{+|-}\ton/off permanent single step mode",
	  &first_char },
	{ 1, "L", "lbr", "%C",
	  "L\tshow last branch recording information\n"
	  "Ld<taskno>\tshow LDT of specific task",
	  &first_char },
    };
  return cs;
}

PUBLIC
int
Jdb_misc_debug::num_cmds() const override
{
  return 2;
}

IMPLEMENT
Jdb_misc_debug::Jdb_misc_debug()
  : Jdb_module("DEBUGGING")
{
}

static Jdb_misc_debug jdb_misc_debug INIT_PRIORITY(JDB_MODULE_INIT_PRIO);


// --------------------------------------------------------
IMPLEMENTATION[(ia32 || amd64) && no_ldt]:

static inline void
Jdb_misc_debug::show_ldt()
{
  printf(" -- no LDT support\n");
}

// --------------------------------------------------------
IMPLEMENTATION[(ia32 || amd64) && !no_ldt]:

static void
Jdb_misc_debug::show_ldt()
{
  Space *s = cxx::dyn_cast<Task*>(reinterpret_cast<Kobject*>(task));

  if (!s)
    {
      printf(" -- invalid task number '%lx'", task);
      return;
    }

  Address addr = s->_ldt.addr();
  Address size = s->_ldt.size();

  if (!size)
    {
      printf(" -- no LDT active");
      return;
    }

  printf("\nLDT of space %lx at " L4_PTR_FMT "-" L4_PTR_FMT "\n",
         task, addr, addr + size - 1);

  Gdt_entry *desc = reinterpret_cast<Gdt_entry *>(addr);

  for (; size >= Cpu::Ldt_entry_size; size -= Cpu::Ldt_entry_size, ++desc)
    {
      if (desc->present())
        {
          printf(" %5lx: ", (Mword)desc - addr);
          desc->show();
        }
    }
}

// --------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

class Jdb_misc_info : public Jdb_module
{
public:
  Jdb_misc_info() FIASCO_INIT;
private:
  static char       first_char;
  static Address    addr;
  static Mword      value;
  static Unsigned64 value64;
};

char       Jdb_misc_info::first_char;
Address    Jdb_misc_info::addr;
Mword      Jdb_misc_info::value;
Unsigned64 Jdb_misc_info::value64;

PUBLIC
Jdb_module::Action_code
Jdb_misc_info::action(int cmd, void *&args, char const *&fmt, int &) override
{
  switch (cmd)
    {
    case 0:
      // read/write physical memory
      if (args == &first_char)
	{
	  if (first_char == 'r' || first_char == 'w')
	    {
	      putchar(first_char);
	      fmt  = L4_ADDR_INPUT_FMT;
	      args = &addr;
	      return EXTRA_INPUT;
	    }
	}
      else if (args == &addr || args == &value)
	{
	  addr &= ~(sizeof(Mword)-1);
	  if (args == &value)
	    Jdb::poke_phys(addr, &value, sizeof(value));
	  if (first_char == 'w' && args == &addr)
	    putstr(" (");
	  else
	    putstr(" => ");
	  Jdb::peek_phys(addr, &value, sizeof(value));
	  printf(L4_MWORD_FMT, value);
	  if (first_char == 'w' && args == &addr)
	    {
	      putstr(") new value=");
	      fmt  = L4_MWORD_FMT;
	      args = &value;
	      return EXTRA_INPUT;
	    }
	  putchar('\n');
	}
      break;

    case 1:
      // read/write machine status register
      if (!Cpu::boot_cpu()->can_wrmsr())
	{
	  puts("MSR not supported");
	  return NOTHING;
	}

      if (args == &first_char)
	{
	  if (first_char == 'r' || first_char == 'w')
	    {
	      putchar(first_char);
	      fmt  = L4_ADDR_INPUT_FMT;
	      args = &addr;
	      return EXTRA_INPUT;
	    }
	}
      if (args == &value64)
        {
          if (!Jdb::wrmsr(value64, addr))
            puts(" => not implemented");
          else
            putchar('\n');
        }
      else if (args == &addr)
	{
          if (!Jdb::rdmsr(addr, &value64))
            putstr(" => not implemented");
          else
            {
              if (first_char == 'r')
                putstr(" => ");
              else
                putstr(" (");
              printf(L4_X64_FMT, value64);
              if (first_char == 'w')
                {
                  putstr(") new value=");
                  fmt  = L4_X64_FMT;
                  args = &value64;
                  return EXTRA_INPUT;
                }
            }
          putchar('\n');
	}
      break;
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_misc_info::cmds() const override
{
  static Cmd cs[] =
    {
	{ 0, "A", "adapter", "%C",
	  "A{r|w}<addr>\tread/write any physical address",
	  &first_char },
        { 1, "M", "msr", "%C",
	  "M{r|w}<addr>\tread/write machine status register",
	  &first_char },
    };
  return cs;
}

PUBLIC
int
Jdb_misc_info::num_cmds() const override
{
  return 2;
}

IMPLEMENT
Jdb_misc_info::Jdb_misc_info()
  : Jdb_module("INFO")
{
}

static Jdb_misc_info jdb_misc_info INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
