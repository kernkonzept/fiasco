/* -*- c -*- */
#include "globalconfig.h"
#include "kernel.ld.inc"

phys_offset = kernel_virt_addr - kernel_load_addr;

#if defined CONFIG_BIT32
OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv",
        "elf32-littleriscv")
#define WORD_SIZE 4
#define MWORD LONG
#else
OUTPUT_FORMAT("elf64-littleriscv", "elf64-littleriscv",
        "elf64-littleriscv")
#define WORD_SIZE 8
#define MWORD QUAD
#endif

OUTPUT_ARCH(riscv)

ENTRY(_start)

PHDRS {
  bstrap PT_LOAD;
  kip PT_LOAD;
  koptions PT_LOAD;
  tramp PT_LOAD;
  ktext PT_LOAD;
  kdata PT_LOAD;
  kitext PT_LOAD;
  kidata PT_LOAD;
  l4_kip 0x10;
  l4_koptions 0x11;
}

EXTERN (_start_kernel)
ASSERT (DEFINED(_start_kernel), "invalid _start_kernel address")
#ifdef CONFIG_MP
EXTERN (_tramp_mp_entry)
#else
PROVIDE (_tramp_mp_entry = 0);
#endif

SECTIONS {
  /DISCARD/ : {
    *(.bootstrap.dropped*)
  }

  . = kernel_load_addr;
  _phys_image_start = .;

  .bstrap : {
    MWORD(_start_kernel)
    MWORD(_tramp_mp_entry)
    MWORD(my_kernel_info_page)
    MWORD(_phys_image_start)
    MWORD(_phys_image_end)
    KEEP(*(.bootstrap.info))
    ASSERT (ABSOLUTE(.) == end_of_bootstrap_info, "invalid size of bootstrap.info");

    . = ABSOLUTE(start_of_loader);
    KEEP(*(.bootstrap.text))
  } :bstrap

  . = ALIGN(4K);
  end_of_loader = .;

  . = end_of_loader + phys_offset;
  PROVIDE (_kernel_image_start = .);

  .kip : AT(ADDR(.kip) - phys_offset) {
    *(.kernel_info_page)
    PROVIDE (_initkip_start = .);
    KEEP(*(.initkip.version))
    KEEP(*(.initkip.features))
    KEEP(*(.initkip.features.end))
    PROVIDE (_initkip_end = .);
  } :kip :l4_kip

  . = ALIGN(4K);

  .koptions : AT(ADDR(.koptions) - phys_offset) {
    *(.koptions)
  } :koptions :l4_koptions = 0

  .mp_tramp : AT(ADDR(.mp_tramp) - phys_offset) {
    KEEP(*(.mp_tramp))
  } :tramp

  .text : AT(ADDR(.text) - phys_offset) {
    crt0.o(.text)
    *(.init)
    *(.text SORT(.text.*) .gnu.linkonce.t.*)
    *(.fini)

    PROVIDE (_ecode = .);

    *(.rodata .rodata.* .gnu.linkonce.r.*)
    *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
    *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*)

    . = ALIGN(WORD_SIZE);
    JDB_TABLE(log);
    . = ALIGN(WORD_SIZE);
    JDB_TABLE(typeinfo);

    . = ALIGN(0x40);

    PROVIDE (_etext = .);
  } : ktext

  . = ALIGN(4K);

  .data : AT(ADDR(.data) - phys_offset) {
    __DATA_BEGIN__ = .;
    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)

    /* Constructor and destructor lists, ordered by priority.  The lists
       are traversed from end to start, therefore the *_END__ symbols
       precede the *_LIST__ symbols. */
    . = ALIGN(8);
    __PER_CPU_INIT_ARRAY_START__ = .;
    KEEP (*(.init_array.65534))
    KEEP (*(.init_array.65533))
    KEEP (*(.init_array.65532))
    KEEP (*(.init_array.65531))
    KEEP (*(.init_array.65530))
    KEEP (*(.init_array.65529))
    KEEP (*(.init_array.65528))
    KEEP (*(.init_array.65527))
    KEEP (*(.init_array.65526))
    KEEP (*(.init_array.65525))
    __PER_CPU_INIT_ARRAY_END__ = .;

    __PER_CPU_LATE_INIT_ARRAY_START__ = .;
    KEEP (*(.init_array.65524))
    KEEP (*(.init_array.65523))
    KEEP (*(.init_array.65522))
    KEEP (*(.init_array.65521))
    KEEP (*(.init_array.65520))
    KEEP (*(.init_array.65519))
    KEEP (*(.init_array.65518))
    KEEP (*(.init_array.65517))
    KEEP (*(.init_array.65516))
    KEEP (*(.init_array.65515))
    __PER_CPU_LATE_INIT_ARRAY_END__ = .;

    __INIT_ARRAY_START__ = .;
    KEEP (*(SORT (.init_array.*)))
    KEEP (*(.init_array))
    __INIT_ARRAY_END__ = .;

    __PER_CPU_CTORS_END__ = .;
    KEEP (*(.ctors.00010))
    KEEP (*(.ctors.00009))
    KEEP (*(.ctors.00008))
    KEEP (*(.ctors.00007))
    KEEP (*(.ctors.00006))
    KEEP (*(.ctors.00005))
    KEEP (*(.ctors.00004))
    KEEP (*(.ctors.00003))
    KEEP (*(.ctors.00002))
    KEEP (*(.ctors.00001))
    __PER_CPU_CTORS_LIST__ = .;

    __PER_CPU_LATE_CTORS_END__ = .;
    KEEP (*(.ctors.00020))
    KEEP (*(.ctors.00019))
    KEEP (*(.ctors.00018))
    KEEP (*(.ctors.00017))
    KEEP (*(.ctors.00016))
    KEEP (*(.ctors.00015))
    KEEP (*(.ctors.00014))
    KEEP (*(.ctors.00013))
    KEEP (*(.ctors.00012))
    KEEP (*(.ctors.00011))
    __PER_CPU_LATE_CTORS_LIST__ = .;

    __CTOR_END__ = .;
    CONSTRUCTORS
    KEEP (*(.ctors))
    KEEP (*(SORT (.ctors.*)))
    __CTOR_LIST__ = .;

    __DTOR_END__ = .;
    KEEP (*(SORT (.dtors.*)))
    KEEP (*(.dtors))
    __DTOR_LIST__ = .;

  } : kdata

  .sdata : AT(ADDR(.sdata) - phys_offset) {
    __SDATA_BEGIN__ = .;
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  } : kdata

  PROVIDE (_edata = .);

  . = ALIGN(8);
  .per_cpu_data : AT(ADDR(.per_cpu_data) - phys_offset) {
    PROVIDE (_per_cpu_data_start = .);
    *(.per_cpu.data)
    . = ALIGN(8);
    PROVIDE (_per_cpu_data_end = .);
  } : kdata

  . = ALIGN(0x10);
  .sbss : AT(ADDR(.sbss) - phys_offset) {
    *(.dynsbss)
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon)
    __SBSS_END__ = .;
  } : kdata

  __global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800,
                          MAX(__DATA_BEGIN__ + 0x800, __SBSS_END__ - 0x800));

  .bss : AT(ADDR(.bss) - phys_offset) {
    PROVIDE (_bss_start = .);
    . = ALIGN(WORD_SIZE);
    *(.bss.boot_page_memory)
    _per_cpu_ctor_data_start = .;
    KEEP (*(.bss.per_cpu_ctor_data))
    _per_cpu_ctor_data_end = .;

    *(.bss .bss.* .gnu.linkonce.b.* COMMON)
    PROVIDE (_bss_end = .);
  } : kdata

  . = ALIGN(4K);
  PROVIDE (_end = .);

  .initcall.text : AT(ADDR(.initcall.text) - phys_offset) {
    _initcall_start = .;
    *(.initcall.text*)
  } : kitext

  .initcall.data : AT(ADDR(.initcall.data) - phys_offset) {
    *(.initcall.data*)
    . = ALIGN(4K);
    _initcall_end = .;
  } : kidata

  _phys_image_end = . - phys_offset;

  /DISCARD/ : {
    *(.note)
    *(.note.ABI-tag)
    *(.comment)
    *(.eh_frame)
    *(.dummy)
  }
}
