/* -*- c -*- */
#include "globalconfig.h"
#include "kernel.ld.inc"

#if defined CONFIG_CPU_MIPS64
# define KSEG0    0xffffffff80000000
# define EXC_BASE 0xffffffff80000000
# define WORD_ALIGN 8
#else
# define KSEG0    0x80000000
# define EXC_BASE 0x80000000
# define WORD_ALIGN 4
#endif

#define RAM_BASE 0x00000000
#define EXC_GEN  0x180
#define EXC_TLB_REFILL 0x000
#define EXC_XTLB_REFILL 0x080
#define EXC_ADDR(x) ((EXC_BASE) + (EXC_ ## x))
#define EXC_PHYS(x) EXC_ADDR(x) - KSEG0 + RAM_BASE

#define EXC_SECTION(x)                             \
  .exception.x EXC_ADDR(x) : AT(EXC_PHYS(x)) {     \
    KEEP(*(.exception.x))                          \
  } : exc

ENTRY(_start)

PHDRS {
  exc PT_LOAD;
  kip PT_LOAD;
  rw  PT_LOAD;
  init PT_LOAD;
  l4_kip 0x10;
  l4_kopt 0x11;
}

SECTIONS {

  EXC_SECTION(TLB_REFILL)
  EXC_SECTION(XTLB_REFILL)
  EXC_SECTION(GEN)

  . = kernel_load_addr;
  PROVIDE (_kernel_image_start = .);

  .kip . : AT(ADDR(.kip) - KSEG0) {
    *(.kernel_info_page)
    PROVIDE (_initkip_start = .);
    KEEP(*(.initkip.version))
    KEEP(*(.initkip.features))
    KEEP(*(.initkip.features.end))
    PROVIDE (_initkip_end = .);
    /* See KIP layout in abi/kip.cpp. The feature strings are terminated by '\0'! */
    ASSERT (_initkip_end < my_kernel_info_page + 0x800, "Kernel version strings too long");
  } :kip :l4_kip

  . = ALIGN(16K);

  .mp_tramp : {
    KEEP(*(.mp_tramp))
  } : rw

  .exceptcommon : {
    *(.exceptcommon)
  }

  .text : {
    crt0.o(.text)
    *(.init)
    *(.text .text.* .gnu.linkonce.t.*)

    PROVIDE (_ecode = .);

    *(.rodata .rodata.* .gnu.linkonce.r.*)

    . = ALIGN(WORD_ALIGN);
    PROVIDE(MIPS_cpu_types = .);
    KEEP(*(.mips.cpu_type))
    LONG(0); LONG(0); LONG(0); LONG(0);

    . = ALIGN(WORD_ALIGN);
    JDB_TABLE(log);
    . = ALIGN(WORD_ALIGN);
    JDB_TABLE(typeinfo);

    . = ALIGN(0x40);

    PROVIDE (_etext = .);
  }

  . = ALIGN(4K);

  .data : {
    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)

    *(.init.data)

    . = ALIGN(8);
    DEFINE_INIT_ARRAYS

    PROVIDE (_edata = .);
  }

  . = ALIGN(8);
  .per_cpu_data : {
    PROVIDE (_per_cpu_data_start = .);
    *(.per_cpu.data)
    . = ALIGN(8);
    PROVIDE (_per_cpu_data_end = .);
  }

  . = ALIGN(8);
  .koptions : {
    *(.koptions)
  } :rw :l4_kopt

  . = ALIGN(0x10);
  .bss : {
    PROVIDE (_bss_start = .);
    . = ALIGN(WORD_ALIGN);
    PROVIDE (_init_bss_start = .);
    *(.init.bss .init.bss.*)
    PROVIDE (_init_bss_end = .);
    _per_cpu_ctor_data_start = .;
    KEEP (*(.bss.per_cpu_ctor_data))
    _per_cpu_ctor_data_end = .;

    *(.bss .bss.* .gnu.linkonce.b.* COMMON)
    PROVIDE (_bss_end = .);
  } :rw

  . = ALIGN(4K);

  PROVIDE (_end = .);

/*
    _gp = . + 0x8000;
    .lit8 : {
        *(.lit8)
    }
    .lit4 : {
        *(.lit4)
    }
*/

  . = ALIGN(4096);

  .initcall.text : {
  PROVIDE (_initcall_start = .);
    *(.initcall.text*)
  } :init

  .initcall.data : {
    *(.initcall.data*)
    PROVIDE (_alt_insns_begin = .);
    KEEP(*(SORT(.alt_insns)))
    PROVIDE (_alt_insns_end = .);
    *(.alt_insn_replacement)
    . = ALIGN(4K);
    PROVIDE (_initcall_end = .);
  }

  /DISCARD/ : {
    *(.MIPS.abiflags)
    *(.note)
    *(.note.ABI-tag)
    *(.comment)
    *(.eh_frame) *(.eh_frame_hdr) *(.eh_frame_entry .eh_frame_entry.*)
    *(.dummy)
  }
}
