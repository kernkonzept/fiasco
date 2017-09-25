/* -*- c -*- */

#include "globalconfig.h"
#include "kernel.ld.inc"

/* this file defines the construction of the kernel's virtual address
   space */
#ifdef CONFIG_CPU_VIRT
virt_address       = kernel_load_addr;
#else
virt_address       = 0xf0000000;
#endif
phys_offset        = virt_address - kernel_load_addr;


OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm",
              "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PHDRS {
  bstrap PT_LOAD;
  kip PT_LOAD;
  koptions PT_LOAD;
  tramp PT_LOAD;
  kernel PT_LOAD;
  excpt PT_LOAD;
  initcall PT_LOAD;
  l4_kip 0x10;
  l4_koptions 0x11;
}

SECTIONS {


  /DISCARD/ : {			/* Exit code and data		*/
		*(.exitcall.exit)
	      }
	      
  . = kernel_load_addr + 0x1000;
	      
  .text : {
    bootstrap*(.init)
    bootstrap*(.text .text.* .gnu.linkonce.t.*)
    bootstrap*(.glue_7t) *(.glue_7)
    bootstrap*(.got .got.*)
    bootstrap*(.fini)
    
    bootstrap*(.rodata .rodata.* .gnu.linkonce.r.* .rodata1)

    . = ALIGN(0x40);
    
    bootstrap*(.data .data.* .gnu.linkonce.d.*)
    bootstrap*(.anno)

    bootstrap*(.data) 
    bootstrap*(.gnu.linkonce.d*)
  } :bstrap

  PROVIDE(bootstrap_bss_start = .);
  .bss_bootstrap : {
    bootstrap*(.bss .bss.* COMMON .gnu.linkonce.b.*)
  } :bstrap
  PROVIDE(bootstrap_bss_end = .);

  . = ALIGN(4K);
  end_of_loader = .;

  . = end_of_loader - kernel_load_addr + virt_address;

  .kip : AT (end_of_loader) {
    _kernel_image_start = .;
    *(.kernel_info_page)
    _initkip_start = .;
    KEEP(*(.initkip.version))
    KEEP(*(.initkip.features))
    KEEP(*(.initkip.features.end))
    _initkip_end = .;
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

    . = ALIGN(4);
    JDB_TABLE(log);
    . = ALIGN(4);
    JDB_TABLE(typeinfo);

    . = ALIGN(0x40);
    
    PROVIDE (_etext = .);

    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)

    *(.data) 
    *(.gnu.linkonce.d*)
    *(.anno)
    *(.data.jdb)

    *(.init.data)

/* Constructor and destructor lists, ordered by priority.  The lists
   are traversed from end to start, therefore the *_END__ symbols
   precede the *_LIST__ symbols. */
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
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    __DTOR_LIST__ = .;

    PROVIDE(_edata  =  .);

  } :kernel

  . = ALIGN(8);
  .per_cpu_data : AT (ADDR(.per_cpu_data) - phys_offset) {
    PROVIDE (_per_cpu_data_start = .);
    *(.per_cpu.data)
    . = ALIGN(8);
    PROVIDE (_per_cpu_data_end = .);
  }

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
  }
  __end_of_the_kernel = .;

  ivt_start = ADDR(.bss) + SIZEOF(.bss) - phys_offset;
  .excp 0xffff0000 : AT (ivt_start) {
    KEEP (*(.vect)) *(.excp.text)
    *(.vect.extra)
  } :excpt

  . = __end_of_the_kernel + SIZEOF(.excp);

  PROVIDE( _end = . );
    
  . = ALIGN(4K);
  .initcall : AT(ADDR(.initcall) - phys_offset) {
    _initcall_start = .;
    *(.initcall.*)
    . = ALIGN(4K);
    _initcall_end = .;
  } :initcall

  /DISCARD/ :{
    *(.stab .stabstr .stab.excl .stab.exclstr)
    *(.stab.index .stab.indexstr .comment)
  }

}
