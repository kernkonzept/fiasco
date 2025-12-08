PHDRS
{
  phdr PT_PHDR PHDRS;
  rw PT_LOAD PHDRS;
  dyn PT_DYNAMIC;
}

SECTIONS {
  . = kernel_load_addr + 0x1000;
  .bootstrap.dropped_info : {
    *(.bootstrap.info)
    end_of_bootstrap_info = ABSOLUTE(.);
  } :rw

  .bootstrap.text : ALIGN(0x10) {
    start_of_loader = ABSOLUTE(.);
    *(.init)
    *(.text .text.* .gnu.linkonce.t.*)
    *(.glue_7t) *(.glue_7)

    *(.rodata .rodata.* .gnu.linkonce.r.* .rodata1)

    . = ALIGN(0x40);
    *(.data .data.* .gnu.linkonce.d.*)
    *(.anno)

    *(.bss .bss.* COMMON .gnu.linkonce.b.*)

    PROVIDE (_alt_insns_begin = .);
    KEEP(*(SORT(.alt_insns)))
    PROVIDE (_alt_insns_end = .);
    ASSERT(_alt_insns_begin == _alt_insns_end,
           "No alternative instruction patching in bootstrap!");
  } :rw

  /* Add dynamic symbol information. It's dropped later but keeps readelf happy
   * when dumping bootstrap.fiasco.pre.o... */
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }

  .rel.dyn        :
    {
      *(.rel.init)
      *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
      *(.rel.fini)
      *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
      *(.rel.data.rel.ro .rel.data.rel.ro.* .rel.gnu.linkonce.d.rel.ro.*)
      *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
      *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
      *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
      *(.rel.ctors)
      *(.rel.dtors)
      *(.rel.got)
      *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
      *(.rel.iplt)
    }
  .rela.dyn       :
    {
      *(.rela.init)
      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
      *(.rela.fini)
      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
      *(.rela.data.rel.ro .rela.data.rel.ro.* .rela.gnu.linkonce.d.rel.ro.*)
      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
      *(.rela.ctors)
      *(.rela.dtors)
      *(.rela.got)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
      *(.rela.iplt)
    }

  .rel.plt        : { *(.rel.plt) }
  .rela.plt       : { *(.rela.plt) }
  .plt            : { *(.plt) }
  .iplt           : { *(.iplt) }
  .got            : { *(.got.plt) *(.igot.plt) *(.got) *(.igot) } :rw
  .dynamic        : { *(.dynamic) } :dyn :rw

  end_of_loader = ABSOLUTE(.);
}
