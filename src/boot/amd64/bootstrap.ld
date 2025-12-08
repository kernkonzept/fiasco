SECTIONS
{
  .text : 
    {
      *(.init)
      *(.text .text.* .gnu.linkonce.t.*)
      *(.fini)
      *(.rodata .rodata.* .gnu.linkonce.r.*)
      *(.data .data.* .gnu.linkonce.d.*)
      *(.anno)

      PROVIDE (_alt_insns_begin = .);
      KEEP(*(SORT(.alt_insns)))
      PROVIDE (_alt_insns_end = .);
      ASSERT(_alt_insns_begin == _alt_insns_end,
             "No alternative instruction patching in bootstrap!");
    } = 0x90909090

  .bss :
    {
      *(.bss .bss.* .gnu.linkonce.b.* COMMON)
    }
}
