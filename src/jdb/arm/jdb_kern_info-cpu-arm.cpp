IMPLEMENTATION [arm && 32bit]:

#include <cstdio>
#include <cstring>

#include "config.h"
#include "globals.h"
#include "space.h"

class Jdb_kern_info_cpu : public Jdb_kern_info_module
{
private:
  static Mword jdb_mrc_insn(unsigned r_val) asm ("jdb_mrc_insn");
};

static Jdb_kern_info_cpu k_c INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_cpu::Jdb_kern_info_cpu()
  : Jdb_kern_info_module('c', "CPU features")
{
  Jdb_kern_info::register_subcmd(this);
}

asm (
  ".section \".text.jdb\"                     \t\n"
  ".global jdb_mrc_insn                       \t\n"
  "jdb_mrc_insn:   mrc p0, 0, r0, c0, c0, 0   \t\n"
  "                mov pc, lr                 \t\n"
  ".previous                                  \t\n");

PRIVATE static
Mword
Jdb_kern_info_cpu::mrc(Mword insn)
{
  char *m = reinterpret_cast<char *>(jdb_mrc_insn);
  *reinterpret_cast<Mword *>(m) = insn;
  Mem_unit::flush_cache(m, m);
  Mem::isb();
  return jdb_mrc_insn(0);
}

PRIVATE static
bool
Jdb_kern_info_cpu::mrc(unsigned cp_num, unsigned opcode_1, unsigned CRn,
                       unsigned CRm, unsigned opcode_2, Mword *result)
{
  Mword insn = 0xee100010
             | (cp_num   & 0xf) << 8
             | (opcode_1 & 0x7) << 21
	     | (CRn      & 0xf) << 16
	     | (CRm      & 0xf)
	     | (opcode_2 & 0x7) << 5;

  // Set to 0 by the JDB trap handler if jdb_mrc_insn() traps.
  Jdb::sysreg_fail_pc = reinterpret_cast<Address>(jdb_mrc_insn);
  *result = mrc(insn);
  bool success = Jdb::sysreg_fail_pc != 0;
  Jdb::sysreg_fail_pc = 0;
  return success;
}

template<
  unsigned cp_num,
  unsigned opcode_1,
  unsigned CRn,
  unsigned CRm,
  unsigned opcode_2
>
struct Arm_cp_op
{
  enum
  {
    Mrc =  0xee100010
      | (cp_num   & 0xf) << 8
      | (opcode_1 & 0x7) << 21
      | (CRn      & 0xf) << 16
      | (CRm      & 0xf)
      | (opcode_2 & 0x7) << 5,

  };
};


struct Cp_struct {
  Mword mrc_opcode;
  const char *descr;
  void (*show)(Mword val);
};

static
void
show_cache_type(Mword val)
{
  Mword dcache_size = 512 << ((val >> 18) & 0xf);
  Mword icache_size = 512 << ((val >>  6) & 0xf);
  printf("  D-size: %ld   dP: %ld  I-size: %ld iP: %ld\n",
         dcache_size, ((val >> 23) & 1),
         icache_size, ((val >> 11) & 1));
}

static Cp_struct cp_vals[] =
{
  { Arm_cp_op<15, 0, 0, 0, 0>::Mrc, "Main ID",    0 },
  { Arm_cp_op<15, 0, 0, 0, 1>::Mrc, "Cache type", show_cache_type },
};

PRIVATE static
void
Jdb_kern_info_cpu::show_padded_name(char const *name)
{
  enum : int { Pad_size = 47 };
  int sz = min<int>(Pad_size, strlen(name));
  printf("%s:%*s", name, Pad_size - sz, "");
}

PRIVATE static
void
Jdb_kern_info_cpu::show_mrc(unsigned cp_num, unsigned opcode_1, unsigned CRn,
                            unsigned CRm, unsigned opcode_2, char const *name)
{
  show_padded_name(name);
  Mword val;
  if (mrc(cp_num, opcode_1, CRn, CRm, opcode_2, &val))
    printf("%08lx\n", val);
  else
    printf("n/a\n");
}

PUBLIC
void
Jdb_kern_info_cpu::show() override
{

  for (unsigned i = 0; i < cxx::size(cp_vals); ++i)
    {
      Mword val = mrc(cp_vals[i].mrc_opcode);
      show_padded_name(cp_vals[i].descr);
      printf("%08lx\n", val);
      if (cp_vals[i].show)
	cp_vals[i].show(val);
    }

  show_mrc(15, 0,  0,  0, 2, "TCM Status");
  show_mrc(15, 0,  0,  0, 3, "TLB Type");
  show_mrc(15, 0,  0,  1, 0, "Processor Feature Register 0");
  show_mrc(15, 0,  0,  1, 1, "Processor Feature Register 1");
  show_mrc(15, 0,  0,  1, 2, "Debug Feature Register 0");
  show_mrc(15, 0,  0,  1, 3, "Auxiliary Feature Register 0");
  show_mrc(15, 0,  0,  1, 4, "Memory Model Feature Register 0");
  show_mrc(15, 0,  0,  1, 5, "Memory Model Feature Register 1");
  show_mrc(15, 0,  0,  1, 6, "Memory Model Feature Register 2");
  show_mrc(15, 0,  0,  1, 7, "Memory Model Feature Register 3");
  show_mrc(15, 0,  0,  2, 0, "Instruction Set Attribute Reg 0");
  show_mrc(15, 0,  0,  2, 1, "Instruction Set Attribute Reg 1");
  show_mrc(15, 0,  0,  2, 2, "Instruction Set Attribute Reg 2");
  show_mrc(15, 0,  0,  2, 3, "Instruction Set Attribute Reg 3");
  show_mrc(15, 0,  0,  2, 4, "Instruction Set Attribute Reg 4");
  show_mrc(15, 0,  0,  2, 5, "Instruction Set Attribute Reg 5");
  show_mrc(15, 0,  1,  0, 0, "Control Register");
  show_mrc(15, 0,  1,  0, 1, "Auxiliary Control Register");
  show_mrc(15, 0,  1,  0, 2, "Coprocessor Access Control Reg");
  show_mrc(15, 0,  1,  1, 0, "Secure Configuration Register");
  show_mrc(15, 0,  1,  1, 1, "Secure Debug Enable Register");
  show_mrc(15, 0,  1,  1, 2, "Non-Secure Access Control Reg");
  show_mrc(15, 0,  2,  0, 0, "Translation Table Base Reg 0");
  show_mrc(15, 0,  2,  0, 1, "Translation Table Base Reg 1");
  show_mrc(15, 0,  2,  0, 2, "Translation Table Base Control Reg");
  show_mrc(15, 0,  3,  0, 0, "Domain Access Control Register");
  show_mrc(15, 0,  5,  0, 0, "Data Fault Status Register");
  show_mrc(15, 0,  5,  0, 1, "Instruction Fault Status Register");
  show_mrc(15, 0,  6,  0, 0, "Fault Address Register");
  show_mrc(15, 0,  6,  0, 2, "Instruction Fault Address Register");
  show_mrc(15, 0,  9,  0, 0, "Data Cache Lockdown Register");
  show_mrc(15, 0,  9,  0, 1, "Instruction Cache Lockdown Register");
  show_mrc(15, 0,  9,  1, 0, "Data TCM Region Register");
  show_mrc(15, 0,  9,  1, 1, "Instruction TCM Region Register");
  show_mrc(15, 0,  9,  1, 2, "Data TCM Non-secure Control Access Reg");
  show_mrc(15, 0,  9,  1, 3, "Instruction TCM Non-secure Control Access Reg");
  show_mrc(15, 0,  9,  2, 0, "TCM Selection Register");
  show_mrc(15, 0,  9,  8, 0, "Cache Behavior Override Register");
  show_mrc(15, 0, 10,  0, 0, "TLB Lockdown Register");
  show_mrc(15, 0, 10,  2, 0, "Primary Region Remap Register");
  show_mrc(15, 0, 10,  2, 1, "Normal Region Remap Register");
  show_mrc(15, 0, 11,  0, 0, "DMA Ident and Status Register present");
  show_mrc(15, 0, 11,  0, 1, "DMA Ident and Status Register queued");
  show_mrc(15, 0, 11,  0, 2, "DMA Ident and Status Register running");
  show_mrc(15, 0, 11,  0, 3, "DMA Ident and Status Register interrupting");
  show_mrc(15, 0, 11,  1, 0, "DMA User Accessibility Register");
  show_mrc(15, 0, 11,  2, 0, "DMA Channel Number Register");
  show_mrc(15, 0, 11,  4, 0, "DMA Control Register");
  show_mrc(15, 0, 11,  5, 0, "DMA Internal Start Address Register");
  show_mrc(15, 0, 11,  6, 0, "DMA External Start Address Register");
  show_mrc(15, 0, 11,  7, 0, "DMA Internal End Address Register");
  show_mrc(15, 0, 11,  8, 0, "DMA Channel Status Register");
  show_mrc(15, 0, 11, 15, 0, "DMA Context ID Register");
  show_mrc(15, 0, 12,  0, 0, "Secure or Non-Secure Vector Base Address Reg");
  show_mrc(15, 0, 12,  0, 1, "Monitor Vector Base Address Register");
  show_mrc(15, 0, 12,  1, 0, "Interrupt Status Register");
  show_mrc(15, 0, 13,  0, 0, "FCSE PID Register");
  show_mrc(15, 0, 13,  0, 1, "Context ID Register");
  show_mrc(15, 0, 13,  0, 2, "User Read/Write Thread and Proc. ID Register");
  show_mrc(15, 0, 13,  0, 3, "User Read Only Thread and Proc. ID Register");
  show_mrc(15, 0, 13,  0, 4, "Privileged Only Thread and Proc. ID Register");
  show_mrc(15, 0, 15,  2, 4, "Peripheral Port Memory Remap Register");
  show_mrc(15, 0, 15,  9, 0, "Sec User and Non-sec Access Validation Cntr R");
  show_mrc(15, 0, 15, 12, 0, "Performance Montior Control Register");
  show_mrc(15, 0, 15, 12, 1, "Cycle Counter Register");
  show_mrc(15, 0, 15, 12, 2, "Counter Register 0");
  show_mrc(15, 0, 15, 12, 3, "Counter Register 1");
  show_mrc(15, 0, 15, 14, 0, "System Validation Cache Size Mask Register");
  show_mrc(15, 5, 15,  4, 2, "TLB Lockdown Index Register");
  show_mrc(15, 5, 15,  5, 2, "TLB Lockdown VA Register");
  show_mrc(15, 5, 15,  6, 2, "TLB Lockdown PA Register");
  show_mrc(15, 5, 15,  7, 2, "TLB Lockdown Attributes Register");
}
