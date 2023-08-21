/* -*- c -*- */

#include "globalconfig.h"
#include "kernel.ld.inc"

/* This file defines the construction of the kernel's virtual address
   space. Has to be kept in sync with
   src/kern/arm/{32,64}/mem_layout_-arm-*.cpp! */
#if defined CONFIG_BIT32
virt_address       = 0xf0000000;
#elif defined CONFIG_CPU_VIRT
virt_address       = 0x0000ffff40000000;
#else
virt_address       = 0xffff000080000000;
#endif
phys_offset        = virt_address - kernel_load_addr;

#if defined CONFIG_BIT32
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm",
              "elf32-littlearm")

OUTPUT_ARCH(arm)
#define WORD_SIZE 4
#define MWORD LONG

#elif defined CONFIG_BIT64

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-bigaarch64",
              "elf64-littleaarch64")

OUTPUT_ARCH(aarch64)
#define WORD_SIZE 8
#define MWORD QUAD
#else
# error Either CONFIG_BIT32 or CONFIG_BIT64 must be set
#endif

#if defined CONFIG_BIT32 && !defined CONFIG_CPU_VIRT
# define NEED_EXCPT
#endif

ENTRY(_start)

PHDRS {
  bstrap PT_LOAD;
  kip PT_LOAD;
  koptions PT_LOAD;
  tramp PT_LOAD;
  kernel PT_LOAD;
#if defined NEED_EXCPT
  excpt PT_LOAD;
#endif
  initcall PT_LOAD;
  dyn PT_DYNAMIC;
  l4_kip 0x10;
  l4_koptions 0x11;
}

EXTERN(_start_kernel)
ASSERT(DEFINED(_start_kernel), "invalid _start_kernel address")

SECTIONS {


  /DISCARD/ : {			/* Exit code and data		*/
		*(.exitcall.exit)
                *(.bootstrap.dropped*)
	      }
	
  . = kernel_load_addr + 0x1000;
	
  .text kernel_load_addr + 0x1000 : {
    MWORD(_start_kernel)
    MWORD(my_kernel_info_page)
    MWORD(_kernel_image_start - phys_offset)
    MWORD(_initcall_end - phys_offset)
    KEEP(*(.bootstrap.info))
    ASSERT (ABSOLUTE(.) == end_of_bootstrap_info, "invalid size of bootstrap.info");

    . = ABSOLUTE(start_of_loader);
    KEEP(*(.bootstrap.text))
  } :bstrap

  /* Dummy dynamic section to appear as PIE to bootstrap. The real one is in
   * .bootstrap.text. */
  .dynamic : {
    QUAD(0)
    QUAD(0)
  } :bstrap :dyn

  . = ALIGN(4K);
  end_of_loader = .;

  . = end_of_loader + phys_offset;

  .kip : AT (end_of_loader) {
    _kernel_image_start = .;
    *(.kernel_info_page)
    _initkip_start = .;
    KEEP(*(.initkip.version))
    KEEP(*(.initkip.features))
    KEEP(*(.initkip.features.end))
    _initkip_end = .;
    /* See KIP layout in abi/kip.cpp. The feature strings are terminated by '\0'! */
    ASSERT (_initkip_end < my_kernel_info_page + 0x800, "Kernel version strings too long");
    . = ALIGN(4K);
  } :kip :l4_kip

  .koptions : AT (ADDR(.koptions) - phys_offset) {
    *(.koptions)
  } :koptions :l4_koptions

  .mp_tramp : AT (ADDR(.mp_tramp) - phys_offset) {
    KEEP(*(.mp_tramp))
  } :tramp

  .text_kernel : AT (ADDR(.text_kernel) - phys_offset) {
    crt0.o(.text)
    *(.init)
    *(.text .text.* .gnu.linkonce.t.*)
    *(.glue_7t) *(.glue_7)
    *(.got .got.*)
    *(.fini)

    *(.text.jdb)

    PROVIDE (_ecode = .);

    *(.rodata .rodata.* .gnu.linkonce.r.* .rodata1)
    KEEP(*(.rodata.log.*))

    . = ALIGN(WORD_SIZE);
    JDB_TABLE(log);
    . = ALIGN(WORD_SIZE);
    JDB_TABLE(typeinfo);

    . = ALIGN(0x40);

    PROVIDE (_etext = .);
  } :kernel

  __llvm_covmap : { KEEP(*(__llvm_covmap)) } :kernel
  __llvm_covfun : { KEEP(*(__llvm_covfun)) } :kernel
  __llvm_prf_data : { KEEP(*(__llvm_prf_data)) } :kernel
  __llvm_prf_names : { KEEP(*(__llvm_prf_names)) } :kernel

  .data_kernel : AT (ADDR(.data_kernel) - phys_offset) {
    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)

    *(.data)
    *(.gnu.linkonce.d*)
    *(.anno)
    *(.data.jdb)

    *(.init.data)

    . = ALIGN(8);
    DEFINE_INIT_ARRAYS

    PROVIDE(_edata  =  .);
  } :kernel

  __llvm_prf_cnts : { KEEP(*(__llvm_prf_cnts)) } :kernel

  . = ALIGN(8);
  .per_cpu_data : AT (ADDR(.per_cpu_data) - phys_offset) {
    PROVIDE (_per_cpu_data_start = .);
    *(.per_cpu.data)
    . = ALIGN(8);
    PROVIDE (_per_cpu_data_end = .);
  } :kernel

  /* ARM AEABI */
  . = ALIGN(8);
  .ARM.exidx : { *(.ARM.exidx.*) }
  .ARM.extab : { *(.ARM.extab.*) }

  . = ALIGN(4K);
  .bss : AT (ADDR(.bss) - phys_offset) {
    PROVIDE(__bss_start = .);
    . = ALIGN(4);
    *(.bss.kernel_page_dir)
    _per_cpu_ctor_data_start = .;
    KEEP (*(.bss.per_cpu_ctor_data))
    _per_cpu_ctor_data_end = .;
    *(.bss .bss.* COMMON .gnu.linkonce.b.*)
    *(.bss.jdb)
    PROVIDE(__bss_end = .);
    . = ALIGN(4K);
  } :kernel
  __end_of_the_kernel = .;

#if defined NEED_EXCPT
  ivt_start = ADDR(.bss) + SIZEOF(.bss) - phys_offset;
  .excp 0xffff0000 : AT (ivt_start) {
    KEEP (*(.vect)) *(.excp.text)
    *(.vect.extra)
  } :excpt

  . = __end_of_the_kernel + SIZEOF(.excp);
#endif

  PROVIDE( _end = . );

  . = ALIGN(4K);
  .initcall : AT (ADDR(.initcall) - phys_offset) {
    _initcall_start = .;
    *(.initcall.*)
    PROVIDE (_alt_insns_begin = .);
    KEEP(*(SORT(.alt_insns)))
    PROVIDE (_alt_insns_end = .);
    *(.alt_insn_replacement)
    . = ALIGN(4K);
    _initcall_end = .;
  } :initcall

  /DISCARD/ :{
    *(.stab .stabstr .stab.excl .stab.exclstr)
    *(.stab.index .stab.indexstr .comment)
  }

}
