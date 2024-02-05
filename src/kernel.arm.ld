/* -*- c -*- */

#include "globalconfig.h"
#include "kernel.ld.inc"

/* This file defines the construction of the kernel's virtual address
   space. Has to be kept in sync with
   src/kern/arm/{32,64}/mem_layout_-arm-*.cpp! */
#if !defined(CONFIG_MMU)
virt_address       = kernel_load_addr;
#elif defined CONFIG_BIT32
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

#if defined CONFIG_BIT32 && !defined CONFIG_CPU_VIRT && defined CONFIG_MMU
# define NEED_EXCPT
#endif

ENTRY(_start)

PHDRS {
  bstrap PT_LOAD;
  kip PT_LOAD;
  koptions PT_LOAD;
  tramp_data PT_LOAD;
  tramp_text PT_LOAD;
  ktext PT_LOAD;
  kdata PT_LOAD;
#if defined NEED_EXCPT
  excpt PT_LOAD;
#endif
  initcall_data PT_LOAD;
#if defined(CONFIG_MMU)
  dyn PT_DYNAMIC;
#endif
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

#if defined(CONFIG_MMU)
  /* Dummy dynamic section to appear as PIE to bootstrap. The real one is in
   * .bootstrap.text. In fact, only the bootstrap stage of Fiasco is position
   * independent. The rest is always linked position dependent. This makes
   * a no-MMU kernel position dependent! */
  .dynamic : {
    QUAD(0)
    QUAD(0)
  } :bstrap :dyn
#endif

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
    // No-MMU kernels map the KIP with a separate region! So the kernel image
    // region actually starts from here in this case.
    _kernel_kip_end = .;
    *(.koptions)
  } :koptions :l4_koptions

  .mp_tramp_data : AT (ADDR(.mp_tramp_data) - phys_offset) {
    KEEP(*(.mp_tramp_data))
  } :tramp_data

  /*
   * With the 2M alignment begins the executable part of the kernel address
   * space.
   */
#if defined CONFIG_KERNEL_NX && defined CONFIG_BIT64
  . = ALIGN(2M);
#endif
  _rx_start = .;
  .mp_tramp_text : AT (ADDR(.mp_tramp_text) - phys_offset) {
    KEEP(*(.mp_tramp_text))
  } :tramp_text

  .text_kernel : AT (ADDR(.text_kernel) - phys_offset) {
    crt0.o(.text)
    *(.init)
    *(.text .text.* .gnu.linkonce.t.*)
    *(.glue_7t) *(.glue_7)
    *(.got .got.*)
    *(.fini)

    /*
     * As we are using 2M-PTEs, there is no point in using one for normal
     * kernel text and another one for initcalls. In our case, there is enough
     * room to put the initcalls under one PTE together with kernel text.
     */
    *(.initcall.text*)
    *(.alt_insn_replacement)

    *(.text.jdb)

    PROVIDE (_ecode = .);

    *(.rodata .rodata.* .gnu.linkonce.r.* .rodata1)
    KEEP(*(.rodata.log.*))

    . = ALIGN(WORD_SIZE);
    JDB_TABLE(typeinfo);

    . = ALIGN(0x40);

    PROVIDE (_etext = .);
  } :ktext

  __llvm_covmap : { KEEP(*(__llvm_covmap)) } :ktext
  __llvm_covfun : { KEEP(*(__llvm_covfun)) } :ktext
  __llvm_prf_data : { KEEP(*(__llvm_prf_data)) } :ktext
  __llvm_prf_names : { KEEP(*(__llvm_prf_names)) } :ktext

  /*
   * This section serves the purpose of taking up the remainder of the last 2M
   * up to a new 2M boundary without taking up any space in the file (the linker
   * script can apparently do this only for writable sections). This way we tell
   * the kernel this memory (i.e. the executable part of the kernel address
   * space) is reserved and should not be used for anything.
   */
  .fill_kernel : AT (ADDR(.fill_kernel) - phys_offset) {
#if defined CONFIG_KERNEL_NX && defined CONFIG_BIT64
    . = ALIGN(2M);
#endif
  }
  _rx_end = .;

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

    . = ALIGN(WORD_SIZE);
    JDB_TABLE(log);

    PROVIDE(_edata  =  .);
  } :kdata

  __llvm_prf_bits : { KEEP(*(__llvm_prf_bits)) } :kdata
  __llvm_prf_cnts : { KEEP(*(__llvm_prf_cnts)) } :kdata

  . = ALIGN(8);
  .per_cpu_data : AT (ADDR(.per_cpu_data) - phys_offset) {
    PROVIDE (_per_cpu_data_start = .);
    *(.per_cpu.data)
    . = ALIGN(8);
    PROVIDE (_per_cpu_data_end = .);
  } :kdata

  /* ARM AEABI */
  . = ALIGN(8);
  .ARM.exidx : { *(.ARM.exidx.*) }
  .ARM.extab : { *(.ARM.extab.*) }

  . = ALIGN(8);
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
  } :kdata
  __end_of_the_kernel = .;

#if defined NEED_EXCPT
  ivt_start = ALIGN(. - phys_offset, 4K);
  .excp 0xffff0000 : AT (ivt_start) {
    KEEP (*(.vect)) *(.excp.text)
    *(.vect.extra)
    . = ALIGN(4K);
  } :excpt

  . = ivt_start + phys_offset + SIZEOF(.excp);
#endif

  PROVIDE( _end = . );

  _initcall_start = .;
  .initcall_data : AT (ADDR(.initcall_data) - phys_offset) {
    *(.initcall.data*)
    PROVIDE (_alt_insns_begin = .);
    KEEP(*(SORT(.alt_insns)))
    PROVIDE (_alt_insns_end = .);
    . = ALIGN(4K);
  } :initcall_data
  _initcall_end = .;

  /DISCARD/ :{
    *(.stab .stabstr .stab.excl .stab.exclstr)
    *(.stab.index .stab.indexstr .comment)
    *(.eh_frame)
  }

}
