/* -*- c -*- */

#include "globalconfig.h"
#include "linking.h"
#include "kernel.ld.inc"

_boot_sys_start = 0x300000;
_kernel_image	= FIASCO_IMAGE_VIRT_START;	/* kernel link address */
_load		= FIASCO_IMAGE_PHYS_START & 0x3fffff;
/*
_load		= 0x1000;	/* skip page 0 to preserve BIOS data and
				 * page 1 to reserve space for the KIP */

/*_fiasco_image_offset = 0xf0000000;*/
_fiasco_image_offset = FIASCO_IMAGE_PHYS_OFFSET;

ENTRY(bootstrap__boot_start)

PHDRS {
  tramp PT_LOAD;
  btext PT_LOAD;
  bdata PT_LOAD;
  kip PT_LOAD;
  koptions PT_LOAD;
  ktext PT_LOAD;
  kdata PT_LOAD;
  kitext PT_LOAD;
  kidata PT_LOAD;
  l4_kip 0x10;
  l4_koptions 0x11;
}

SECTIONS {
  /*
   * The correct section attribute would be NOLOAD but in that case ld.lld
   * would wrongly merge the ".bda" content with the "tramp" output section.
   * The 'INFO' section type is marked as "legacy" by GNU ld but nevertheless
   * is still supported and does the right thing. See also the ld.lld docu:
   * "COPY, INFO, OVERLAY: clear the SHF_ALLOC bit in sh_flags."
   */
  .bda 0x400 (INFO) : {
    . += 0xe;
    ebda_segment = .;
  }

  . = FIASCO_MP_TRAMP_PAGE;
  .realmode_tramp : {
      *(.realmode_tramp*)
      . = ALIGN(0x1000);
  } : tramp = 0x00

  . = _boot_sys_start;
  .text_boot : ALIGN(0x1000) {
    *(.bootstrap.init)
    *(.bootstrap.text .bootstrap.text.* .bootstrap.gnu.linkonce.t.*)
    *(.bootstrap.fini)

    *(.bootstrap.initcall*)
    *(.bootstrap.rodata .bootstrap.rodata.* .bootstrap.gnu.linkonce.r.*)
    *(.bootstrap.data .bootstrap.data.* .bootstrap.gnu.linkonce.d.*)
    *(.bootstrap.anno)

  } : btext

  . = ALIGN(0x10);
  .bss_boot : {
      *(.bootstrap.bss .bootstrap.bss.* .bootstrap.gnu.linkonce.b.*)
  . = ALIGN(0x1000);
  PROVIDE(_boot_sys_end = .);
  } : bdata

  . = _kernel_image + _load;

  .kip : AT(ADDR(.kip) - _fiasco_image_offset) {
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
  } :kip :l4_kip = 0

  .koptions : AT (ADDR(.koptions) - _fiasco_image_offset) {
    *(.koptions)
  } :koptions :l4_koptions = 0

#ifdef CONFIG_ALLOW_RO_TEXT
  . = ALIGN(4K);
#endif
  .text : AT (ADDR(.text) - _fiasco_image_offset) {
    PROVIDE ( _kernel_text_start = . );
    *(SORT(.entry.text.*))
    PROVIDE ( _kernel_text_entry_end = . );
    crt0.o(.text)
    *(.init)
    *(.text SORT(.text.*) .gnu.linkonce.t.*)
    *(.fixup .fixup.*)
    *(.fini)

    PROVIDE (_ecode = .);

    *(.rodata .rodata.* .gnu.linkonce.r.*)

    . = ALIGN(4);
    PROVIDE(__exc_table_start = .);
    KEEP(*(__exc_table __exc_table.*))
    PROVIDE(__exc_table_end = .);

    KEEP(*(.rodata.log.*))

    . = ALIGN(4);
    JDB_TABLE(log);

    . = ALIGN(4);
    JDB_TABLE(typeinfo);

    . = ALIGN(0x40);

    PROVIDE (_etext = .);
  } : ktext = 0x90909090

#ifdef CONFIG_ALLOW_RO_TEXT
  . = ALIGN(4K);
#else
  . = ALIGN(0x10);
#endif
  .data : AT (ADDR(.data) - _fiasco_image_offset) {
    PROVIDE (_kernel_data_start = .);
    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)

    . = ALIGN(8);
    DEFINE_INIT_ARRAYS

    PROVIDE (_edata = .);
  } : kdata

  . = ALIGN(8);
  .per_cpu_data : AT (ADDR(.per_cpu_data) - _fiasco_image_offset) {
    PROVIDE (_per_cpu_data_start = .);
    *(.per_cpu.data)
    . = ALIGN(8);
    PROVIDE (_per_cpu_data_end = .);
  } : kdata

  . = ALIGN(0x10);
  .bss : AT (ADDR(.bss) - _fiasco_image_offset) {
    PROVIDE (_bss_start = .);
    . = ALIGN(4);
    _per_cpu_ctor_data_start = .;
    KEEP (*(.bss.per_cpu_ctor_data))
    _per_cpu_ctor_data_end = .;

    *(.bss .bss.* .gnu.linkonce.b.* COMMON)
    PROVIDE (_bss_end = .);
  } : kdata

  PROVIDE (_end = .);

  . = ALIGN(4096);
  PROVIDE (_initcall_start = .);
  .initcall.text : AT (ADDR(.initcall.text) - _fiasco_image_offset) {
    *(.initcall.text*)
  } : kitext = 0x90909090

  .initcall.data : AT (ADDR(.initcall.data) - _fiasco_image_offset) {
    *(.initcall.data*)
    . = ALIGN(4096);
    PROVIDE (_initcall_end = .);
  } : kidata

  ASSERT(_initcall_end - _kernel_image < FIASCO_IMAGE_VIRT_SIZE,
         "Increase FIASCO_IMAGE_VIRT_SIZE!")

  /DISCARD/ : {
    *(.note)
    *(.note.ABI-tag)
    *(.comment)
    *(.eh_frame)
  }
}
