#pragma once

#include "globalconfig.h"

#ifndef __ASSEMBLER__
# define __RISCV_ASM_INST(inst) #inst
#else
# define __RISCV_ASM_INST(inst) inst
#endif

#if __riscv_xlen == 64
  #define SZREG	8
  #define SZMOD __RISCV_ASM_INST(d)
  #define REG_S __RISCV_ASM_INST(sd)
  #define REG_L __RISCV_ASM_INST(ld)
  #define REG_LR __RISCV_ASM_INST(lr.d)
  #define REG_SC __RISCV_ASM_INST(sc.d)
#elif __riscv_xlen == 32
  #define SZREG	4
  #define SZMOD __RISCV_ASM_INST(w)
  #define REG_S __RISCV_ASM_INST(sw)
  #define REG_L __RISCV_ASM_INST(lw)
  #define REG_LR __RISCV_ASM_INST(lr.w)
  #define REG_SC __RISCV_ASM_INST(sc.w)
#else
  #error __riscv_xlen must be either 32 or 64
#endif

#ifdef CONFIG_RISCV_FPU_SINGLE
  #define FREG_S __RISCV_ASM_INST(fsw)
  #define FREG_L __RISCV_ASM_INST(flw)
  #define SZFREG 4
#elif defined(CONFIG_RISCV_FPU_DOUBLE)
  #define FREG_S __RISCV_ASM_INST(fsd)
  #define FREG_L __RISCV_ASM_INST(fld)
  #define SZFREG 8
#endif

#define ENTRY(symbol)       \
    .globl  symbol;         \
    .align  2;              \
    .type symbol,@function; \
symbol:

#undef END
#define END(function)         \
    .size function,.-function
