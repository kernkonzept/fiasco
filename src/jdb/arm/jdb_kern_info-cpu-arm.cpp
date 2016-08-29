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

PRIVATE
Mword
Jdb_kern_info_cpu::mrc(Mword insn)
{
  char *m = reinterpret_cast<char *>(jdb_mrc_insn);
  *reinterpret_cast<Mword *>(m) = insn;
  Mem_unit::flush_cache(m, m);
  Mem::isb();
  return jdb_mrc_insn(0);
}

PRIVATE
Mword
Jdb_kern_info_cpu::mrc(unsigned cp_num, unsigned opcode_1,
                       unsigned CRn, unsigned CRm, unsigned opcode_2)
{
  Mword insn = 0xee100010
             | (cp_num   & 0xf) << 8
             | (opcode_1 & 0x7) << 21
	     | (CRn      & 0xf) << 16
	     | (CRm      & 0xf)
	     | (opcode_2 & 0x7) << 5;

  return mrc(insn);
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

static Cp_struct cp_vals[] = {
  { Arm_cp_op<15, 0, 0, 0, 0>::Mrc, "Main ID",    0 },
  { Arm_cp_op<15, 0, 0, 0, 1>::Mrc, "Cache type", show_cache_type },
};

PUBLIC
void
Jdb_kern_info_cpu::show()
{

  for (unsigned i = 0; i < sizeof(cp_vals) / sizeof(cp_vals[0]); ++i)
    {
      Mword val = mrc(cp_vals[i].mrc_opcode);
      printf("%47.s: %08lx\n", cp_vals[i].descr, val);
      if (cp_vals[i].show)
	cp_vals[i].show(val);
    }

  printf("Main ID:                                       %08lx\n", mrc(15, 0,  0,  0, 0));
  printf("Cache type:                                    %08lx\n", mrc(15, 0,  0,  0, 1));
  printf("TCM Status:                                    %08lx\n", mrc(15, 0,  0,  0, 2));
  printf("TLB Type:                                      %08lx\n", mrc(15, 0,  0,  0, 3));
  printf("Processor Feature Register 0:                  %08lx\n", mrc(15, 0,  0,  1, 0));
  printf("Processor Feature Register 1:                  %08lx\n", mrc(15, 0,  0,  1, 1));
  printf("Debug Feature Register 0:                      %08lx\n", mrc(15, 0,  0,  1, 2));
  printf("Auxiliary Feature Register 0:                  %08lx\n", mrc(15, 0,  0,  1, 3));
  printf("Memory Model Feature Register 0:               %08lx\n", mrc(15, 0,  0,  1, 4));
  printf("Memory Model Feature Register 1:               %08lx\n", mrc(15, 0,  0,  1, 5));
  printf("Memory Model Feature Register 2:               %08lx\n", mrc(15, 0,  0,  1, 6));
  printf("Memory Model Feature Register 3:               %08lx\n", mrc(15, 0,  0,  1, 7));
  printf("Instruction Set Attribute Reg 0:               %08lx\n", mrc(15, 0,  0,  2, 0));
  printf("Instruction Set Attribute Reg 1:               %08lx\n", mrc(15, 0,  0,  2, 1));
  printf("Instruction Set Attribute Reg 2:               %08lx\n", mrc(15, 0,  0,  2, 2));
  printf("Instruction Set Attribute Reg 3:               %08lx\n", mrc(15, 0,  0,  2, 3));
  printf("Instruction Set Attribute Reg 4:               %08lx\n", mrc(15, 0,  0,  2, 4));
  printf("Instruction Set Attribute Reg 5:               %08lx\n", mrc(15, 0,  0,  2, 5));
  printf("Control Register:                              %08lx\n", mrc(15, 0,  1,  0, 0));
  printf("Auxiliary Control Register:                    %08lx\n", mrc(15, 0,  1,  0, 1));
  printf("Coprocessor Access Control Reg:                %08lx\n", mrc(15, 0,  1,  0, 2));
  printf("Secure Configuration Register:                 %08lx\n", mrc(15, 0,  1,  1, 0));
  printf("Secure Debug Enable Register:                  %08lx\n", mrc(15, 0,  1,  1, 1));
  printf("Non-Secure Access Control Reg:                 %08lx\n", mrc(15, 0,  1,  1, 2));
  printf("Translation Table Base Reg 0:                  %08lx\n", mrc(15, 0,  2,  0, 0));
  printf("Translation Table Base Reg 1:                  %08lx\n", mrc(15, 0,  2,  0, 1));
  printf("Translation Table Base Control Reg:            %08lx\n", mrc(15, 0,  2,  0, 2));
  printf("Domain Access Control Register:                %08lx\n", mrc(15, 0,  3,  0, 0));
  printf("Data Fault Status Register:                    %08lx\n", mrc(15, 0,  5,  0, 0));
  printf("Instruction Fault Status Register:             %08lx\n", mrc(15, 0,  5,  0, 1));
  printf("Fault Address Register:                        %08lx\n", mrc(15, 0,  6,  0, 0));
  printf("Instruction Fault Address Register:            %08lx\n", mrc(15, 0,  6,  0, 2));
  printf("Data Cache Lockdown Register:                  %08lx\n", mrc(15, 0,  9,  0, 0));
  printf("Instruction Cache Lockdown Register:           %08lx\n", mrc(15, 0,  9,  0, 1));
  printf("Data TCM Region Register:                      %08lx\n", mrc(15, 0,  9,  1, 0));
  printf("Instruction TCM Region Register:               %08lx\n", mrc(15, 0,  9,  1, 1));
  printf("Data TCM Non-secure Control Access Reg:        %08lx\n", mrc(15, 0,  9,  1, 2));
  printf("Instruction TCM Non-secure Control Access Reg: %08lx\n", mrc(15, 0,  9,  1, 3));
  printf("TCM Selection Register:                        %08lx\n", mrc(15, 0,  9,  2, 0));
  printf("Cache Behavior Override Register:              %08lx\n", mrc(15, 0,  9,  8, 0));
  printf("TLB Lockdown Register:                         %08lx\n", mrc(15, 0, 10,  0, 0));
  printf("Primary Region Remap Register:                 %08lx\n", mrc(15, 0, 10,  2, 0));
  printf("Normal Region Remap Register:                  %08lx\n", mrc(15, 0, 10,  2, 1));
  printf("DMA Ident and Status Register present:         %08lx\n", mrc(15, 0, 11,  0, 0));
  printf("DMA Ident and Status Register queued:          %08lx\n", mrc(15, 0, 11,  0, 1));
  printf("DMA Ident and Status Register running:         %08lx\n", mrc(15, 0, 11,  0, 2));
  printf("DMA Ident and Status Register interrupting:    %08lx\n", mrc(15, 0, 11,  0, 3));
  printf("DMA User Accessibility Register:               %08lx\n", mrc(15, 0, 11,  1, 0));
  printf("DMA Channel Number Register:                   %08lx\n", mrc(15, 0, 11,  2, 0));
  printf("DMA Control Register:                          %08lx\n", mrc(15, 0, 11,  4, 0));
  printf("DMA Internal Start Address Register:           %08lx\n", mrc(15, 0, 11,  5, 0));
  printf("DMA External Start Address Register:           %08lx\n", mrc(15, 0, 11,  6, 0));
  printf("DMA Internal End Address Register:             %08lx\n", mrc(15, 0, 11,  7, 0));
  printf("DMA Channel Status Register:                   %08lx\n", mrc(15, 0, 11,  8, 0));
  printf("DMA Context ID Register:                       %08lx\n", mrc(15, 0, 11, 15, 0));
  printf("Secure or Non-Secure Vector Base Address Reg:  %08lx\n", mrc(15, 0, 12,  0, 0));
  printf("Monitor Vector Base Address Register:          %08lx\n", mrc(15, 0, 12,  0, 1));
  printf("Interrupt Status Register:                     %08lx\n", mrc(15, 0, 12,  1, 0));
  printf("FCSE PID Register:                             %08lx\n", mrc(15, 0, 13,  0, 0));
  printf("Context ID Register:                           %08lx\n", mrc(15, 0, 13,  0, 1));
  printf("User Read/Write Thread and Proc. ID Register:  %08lx\n", mrc(15, 0, 13,  0, 2));
  printf("User Read Only Thread and Proc. ID Register:   %08lx\n", mrc(15, 0, 13,  0, 3));
  printf("Privileged Only Thread and Proc. ID Register:  %08lx\n", mrc(15, 0, 13,  0, 4));
  printf("Peripheral Port Memory Remap Register:         %08lx\n", mrc(15, 0, 15,  2, 4));
  printf("Sec User and Non-sec Access Validation Cntr R: %08lx\n", mrc(15, 0, 15,  9, 0));
  printf("Performance Montior Control Register:          %08lx\n", mrc(15, 0, 15, 12, 0));
  printf("Cycle Counter Register:                        %08lx\n", mrc(15, 0, 15, 12, 1));
  printf("Counter Register 0:                            %08lx\n", mrc(15, 0, 15, 12, 2));
  printf("Counter Register 1:                            %08lx\n", mrc(15, 0, 15, 12, 3));
  printf("System Validation Cache Size Mask Register:    %08lx\n", mrc(15, 0, 15, 14, 0));
  printf("TLB Lockdown Index Register:                   %08lx\n", mrc(15, 5, 15,  4, 2));
  printf("TLB Lockdown VA Register:                      %08lx\n", mrc(15, 5, 15,  5, 2));
  printf("TLB Lockdown PA Register:                      %08lx\n", mrc(15, 5, 15,  6, 2));
  printf("TLB Lockdown Attributes Register:              %08lx\n", mrc(15, 5, 15,  7, 2));
}
