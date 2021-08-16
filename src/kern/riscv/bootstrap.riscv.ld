
SECTIONS {
  . = kernel_load_addr;
  .bootstrap.dropped_info : {
    *(.bootstrap.info)
    end_of_bootstrap_info = ABSOLUTE(.);
  }

  .bootstrap.text : ALIGN(0x10) {
    start_of_loader = ABSOLUTE(.);
    *(.init)
    *(.text .text.* .gnu.linkonce.t.*)

    *(.rodata .rodata.* .gnu.linkonce.r.*)
    *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
    *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*)

    . = ALIGN(0x40);
    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)
    __SDATA_BEGIN__ = .;
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
    *(.got .got.*)

    *(.sbss .sbss.* .gnu.linkonce.sb.* .scommon)
    *(.bss .bss.* .gnu.linkonce.b.* COMMON)
  }

  /DISCARD/ : { *(*) }
}
