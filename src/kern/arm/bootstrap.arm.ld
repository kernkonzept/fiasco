
SECTIONS {
  . = kernel_load_addr + 0x1000;
  .bootstrap.dropped_info : {
    *(.bootstrap.info)
    end_of_bootstrap_info = ABSOLUTE(.);
  }

  .bootstrap.text : ALIGN(0x10) {
    start_of_loader = ABSOLUTE(.);
    *(.init)
    *(.text .text.* .gnu.linkonce.t.*)
    *(.text.*)
    *(.glue_7t) *(.glue_7)
    *(.got .got.*)

    *(.rodata .rodata.* .gnu.linkonce.r.* .rodata1)

    . = ALIGN(0x40);
    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)

    *(.bss .bss.* COMMON .gnu.linkonce.b.*)
  }

  /DISCARD/ : { *(*) }
}
