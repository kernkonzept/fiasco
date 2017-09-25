INTERFACE [svm]:

#include "l4_types.h"
#include <cxx/bitfield>

// TODO: Make this sharable with user-land (see uarts)
struct Vmcb_control_area
{
  struct Clean_bits
  {
    Unsigned64 raw;
    CXX_BITFIELD_MEMBER( 0,  0, i, raw);
    CXX_BITFIELD_MEMBER( 1,  1, iopm, raw);
    CXX_BITFIELD_MEMBER( 2,  2, asid, raw);
    CXX_BITFIELD_MEMBER( 3,  3, tpr, raw);
    CXX_BITFIELD_MEMBER( 4,  4, np, raw);
    CXX_BITFIELD_MEMBER( 5,  5, crx, raw);
    CXX_BITFIELD_MEMBER( 6,  6, drx, raw);
    CXX_BITFIELD_MEMBER( 7,  7, dt, raw);
    CXX_BITFIELD_MEMBER( 8,  8, seg, raw);
    CXX_BITFIELD_MEMBER( 9,  9, cr2, raw);
    CXX_BITFIELD_MEMBER(10, 10, lbr, raw);
    CXX_BITFIELD_MEMBER(11, 11, avic, raw);
  };

  Unsigned16 intercept_rd_crX;
  Unsigned16 intercept_wr_crX;

  Unsigned16 intercept_rd_drX;
  Unsigned16 intercept_wr_drX;

  Unsigned32 intercept_exceptions;

  Unsigned32 intercept_instruction0;
  Unsigned32 intercept_instruction1;

  Unsigned8 _reserved0[40];

  Unsigned16 pause_filter_threshold;
  Unsigned16 pause_filter_count;

  Unsigned64 iopm_base_pa;
  Unsigned64 msrpm_base_pa;
  Unsigned64 tsc_offset;
  Unsigned32 guest_asid;
  Unsigned32 tlb_ctl;
  Unsigned16 int_tpr_irq;
  Unsigned16 int_ctl;
  Unsigned8  int_vector;
  Unsigned8  _reserved10[3];
  Unsigned8  interrupt_shadow;
  Unsigned8  _reserved11[7];
  Unsigned64 exitcode;
  Unsigned64 exitinfo1;
  Unsigned64 exitinfo2;
  Unsigned64 exitintinfo;
  Unsigned64 np_enable;

  Unsigned8 _reserved1[16];

  Unsigned64 eventinj;
  Unsigned64 n_cr3;
  Unsigned64 lbr_virtualization_enable;
  Clean_bits clean_bits;
  Unsigned64 n_rip;

  Unsigned8 _reserved2[816];
} __attribute__((packed));

struct Vmcb_state_save_area
{
  Unsigned16 es_sel;
  Unsigned16 es_attrib;
  Unsigned32 es_limit;
  Unsigned64 es_base;

  Unsigned16 cs_sel;
  Unsigned16 cs_attrib;
  Unsigned32 cs_limit;
  Unsigned64 cs_base;

  Unsigned16 ss_sel;
  Unsigned16 ss_attrib;
  Unsigned32 ss_limit;
  Unsigned64 ss_base;

  Unsigned16 ds_sel;
  Unsigned16 ds_attrib;
  Unsigned32 ds_limit;
  Unsigned64 ds_base;

  Unsigned16 fs_sel;
  Unsigned16 fs_attrib;
  Unsigned32 fs_limit;
  Unsigned64 fs_base;

  Unsigned16 gs_sel;
  Unsigned16 gs_attrib;
  Unsigned32 gs_limit;
  Unsigned64 gs_base;

  Unsigned16 gdtr_sel;
  Unsigned16 gdtr_attrib;
  Unsigned32 gdtr_limit;
  Unsigned64 gdtr_base;

  Unsigned16 ldtr_sel;
  Unsigned16 ldtr_attrib;
  Unsigned32 ldtr_limit;
  Unsigned64 ldtr_base;

  Unsigned16 idtr_sel;
  Unsigned16 idtr_attrib;
  Unsigned32 idtr_limit;
  Unsigned64 idtr_base;

  Unsigned16 tr_sel;
  Unsigned16 tr_attrib;
  Unsigned32 tr_limit;
  Unsigned64 tr_base;

  Unsigned8 _reserved0[43];

  Unsigned8 cpl;

  Unsigned32 _reserved1;

  Unsigned64 efer;

  Unsigned8 _reserved2[112];

  Unsigned64 cr4;
  Unsigned64 cr3;
  Unsigned64 cr0;
  Unsigned64 dr7;
  Unsigned64 dr6;
  Unsigned64 rflags;
  Unsigned64 rip;

  Unsigned8 _reserved3[88];

  Unsigned64 rsp;

  Unsigned8 _reserved4[24];

  Unsigned64 rax;
  Unsigned64 star;
  Unsigned64 lstar;
  Unsigned64 cstar;
  Unsigned64 sfmask;
  Unsigned64 kernelgsbase;
  Unsigned64 sysenter_cs;
  Unsigned64 sysenter_esp;
  Unsigned64 sysenter_eip;
  Unsigned64 cr2;

  Unsigned8 _reserved5[32];

  Unsigned64 g_pat;
  Unsigned64 dbgctl;
  Unsigned64 br_from;
  Unsigned64 br_to;
  Unsigned64 lastexcpfrom;
  Unsigned64 last_excpto;

  // this field is _NOT_ part of AMDs hardware VMCB specification
  // this is a software VMCB field
  Unsigned64 xcr0;

  Unsigned8 _reserved6[2400];
} __attribute__((packed));

struct Vmcb
{
  unsigned np_enabled() const { return control_area.np_enable & 1; }

  Vmcb_control_area control_area;
  Vmcb_state_save_area state_save_area;
} __attribute__((packed));
