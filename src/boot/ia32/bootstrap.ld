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
    } = 0x90909090

  .bss :
    {
      *(.bss .gnu.linkonce.b.* COMMON)
    }

}
