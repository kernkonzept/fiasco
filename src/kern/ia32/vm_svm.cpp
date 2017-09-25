INTERFACE [svm]:

#include "config.h"
#include "vm.h"

struct Vmcb;
class Svm;

class Vm_svm : public Vm
{
private:
  static void resume_vm_svm(Mword phys_vmcb, Trex *regs)
    asm("resume_vm_svm") __attribute__((__regparm__(3)));

  struct Asid_info
  {
    Unsigned32 asid;
    Unsigned32 generation;
  };

  typedef Per_cpu_array<Asid_info> Asid_array;

  Asid_array _asid;

  enum
  {
    EFER_LME = 1 << 8,
    EFER_LMA = 1 << 10,
  };
};

// ------------------------------------------------------------------------
INTERFACE [svm && debug]:

#include "tb_entry.h"

EXTENSION class Vm_svm
{
protected:
  struct Log_vm_svm_exit : public Tb_entry
  {
    Mword exitcode, exitinfo1, exitinfo2, rip;
    void print(String_buffer *buf) const;
  };

};

// ------------------------------------------------------------------------
IMPLEMENTATION [svm]:

#include "context.h"
#include "mem_space.h"
#include "fpu.h"
#include "svm.h"
#include "thread.h" // XXX: circular dep, move this out here!
#include "thread_state.h" // XXX: circular dep, move this out here!


// ------------------------------------------------------------------------
IMPLEMENTATION [svm && ia32]:

#include "virt.h"

PRIVATE inline
void
Vm_svm::restore_segments(Context *, Unsigned16 fs, Unsigned16 gs)
{
  Cpu::set_fs(fs);
  Cpu::set_gs(gs);
}

PRIVATE inline NEEDS["virt.h"]
Address
Vm_svm::get_vm_cr3(Vmcb *)
{
  // When running in 32bit mode we already return the page-table of our Vm
  // object, whether we're running with shadow or nested paging
  return phys_dir();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [svm && amd64]:

#include "assert_opt.h"
#include "virt.h"

PRIVATE inline
void
Vm_svm::restore_segments(Context *ctxt, Unsigned16 fs, Unsigned16 gs)
{
  Cpu::set_fs(fs);
  if (!fs)
    Cpu::wrmsr(ctxt->fs_base(), MSR_FS_BASE);

  Cpu::set_gs(gs);
  if (!gs)
    Cpu::wrmsr(ctxt->gs_base(), MSR_GS_BASE);
}

PRIVATE inline NEEDS["assert_opt.h", "virt.h"]
Address
Vm_svm::get_vm_cr3(Vmcb *v)
{
  // When we have nested paging, we just return the 4lvl host page-table of
  // our Vm.
  if (v->np_enabled())
     return phys_dir();

  // When running with shadow paging and the guest is running in long mode
  // and has paging enabled, we can just return the 4lvl page table of our
  // host Vm object.
  if (   (v->state_save_area.efer & EFER_LME)
      && (v->state_save_area.cr0 & CR0_PG))
    return phys_dir();

  // Now it's getting tricky when running with shadow paging.
  // We need to obey the following rules:
  //  - When the guest is not running in 64bit mode the CR3 one can set for
  //    the page-table must be below 4G physical memory (i.e. bit 32-63 must
  //    be zero). This is unfortunate when the host has memory above 4G as
  //    Fiasco gets its memory from the end of physical memory, i.e.
  //    page-table memory is above 4G.
  //  - We need an appropriate page-table format for 32bit!
  //    That means either a 2lvl page-table or a 3lvl PAE one. That would
  //    require to maintain two page-tables for the guest, one for 32bit
  //    mode execution and one for 64 bit execution. It is needed either for
  //    the transition from real to long-mode via protected mode or for
  //    32bit only guests.
  //    There's one trick to avoid having two PTs: 4lvl-PTs and 3lvl-PAE-PTs
  //    have much in common so that it's possible to just take the the PDPE
  //    one of the host as the 3lvl-PAE-PT for the guest. Well, not quite.
  //    The problem is that SVM checks that MBZ bits in the PAE-PT entries
  //    are really 0 as written in the spec. Now the 4lvl PT contains rights
  //    bits there, so that this type of PT is refused and does not work on
  //    real hardware.
  //    So why is the code still here? Well, QEmu isn't so picky about the
  //    bits in the PDPE and it thus works there...
  assert_opt (this);
  Address vm_cr3 = static_cast<Mem_space*>(this)->dir()->walk(Virt_addr(0), 0).next_level();
  if (EXPECT_FALSE(!vm_cr3))
    {
      // force allocation of new secondary page-table level
      static_cast<Mem_space*>(this)->dir()
        ->walk(Virt_addr(0), 1, false, Kmem_alloc::q_allocator(ram_quota()));
      vm_cr3 = static_cast<Mem_space*>(this)->dir()->walk(Virt_addr(0), 0).next_level();
    }

  if (EXPECT_FALSE(vm_cr3 >= 1UL << 32))
    {
      WARN("svm: Host page-table not under 4G, sorry.\n");
      return 0;
    }

  return vm_cr3;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [svm]:

PRIVATE inline NOEXPORT
void
Vm_svm::new_asid(Cpu_number cpu, Svm *svm)
{
  Asid_info *a = &_asid[cpu];
  a->asid = svm->next_asid();
  if (svm->global_asid_generation() == a->generation)
    a->generation = svm->global_asid_generation();
}

PRIVATE inline NOEXPORT
Vm_svm::Asid_info const *
Vm_svm::asid(Cpu_number cpu) const
{ return &_asid[cpu]; }

PUBLIC
Vm_svm::Vm_svm(Ram_quota *q)
  : Vm(q)
{
  memset(&_asid, 0, sizeof(_asid));
}

PUBLIC inline
void *
Vm_svm::operator new (size_t size, void *p) throw()
{
  (void)size;
  assert (size == sizeof (Vm_svm));
  return p;
}

PUBLIC
void
Vm_svm::operator delete (void *ptr)
{
  Vm_svm *t = reinterpret_cast<Vm_svm*>(ptr);
  Kmem_slab_t<Vm_svm>::q_free(t->ram_quota(), ptr);
}


// to do:
//   - handle cr2
//   - force fpu ownership
//   - debug registers not covered by VMCB

PRIVATE inline
void
Vm_svm::copy_state_save_area(Vmcb *dest, Vmcb *src)
{
  Vmcb_state_save_area *d = &dest->state_save_area;
  Vmcb_state_save_area *s = &src->state_save_area;

  d->es_sel       = s->es_sel;
  d->es_attrib    = s->es_attrib;
  d->es_limit     = s->es_limit;
  d->es_base      = s->es_base;

  d->cs_sel       = s->cs_sel;
  d->cs_attrib    = s->cs_attrib;
  d->cs_limit     = s->cs_limit;
  d->cs_base      = s->cs_base;

  d->ss_sel       = s->ss_sel;
  d->ss_attrib    = s->ss_attrib;
  d->ss_limit     = s->ss_limit;
  d->ss_base      = s->ss_base;

  d->ds_sel       = s->ds_sel;
  d->ds_attrib    = s->ds_attrib;
  d->ds_limit     = s->ds_limit;
  d->ds_base      = s->ds_base;

  d->fs_sel       = s->fs_sel;
  d->fs_attrib    = s->fs_attrib;
  d->fs_limit     = s->fs_limit;
  d->fs_base      = s->fs_base;

  d->gs_sel       = s->gs_sel;
  d->gs_attrib    = s->gs_attrib;
  d->gs_limit     = s->gs_limit;
  d->gs_base      = s->gs_base;

  d->gdtr_sel     = s->gdtr_sel;
  d->gdtr_attrib  = s->gdtr_attrib;
  d->gdtr_limit   = s->gdtr_limit;
  d->gdtr_base    = s->gdtr_base;

  d->ldtr_sel     = s->ldtr_sel;
  d->ldtr_attrib  = s->ldtr_attrib;
  d->ldtr_limit   = s->ldtr_limit;
  d->ldtr_base    = s->ldtr_base;

  d->idtr_sel     = s->idtr_sel;
  d->idtr_attrib  = s->idtr_attrib;
  d->idtr_limit   = s->idtr_limit;
  d->idtr_base    = s->idtr_base;

  d->tr_sel       = s->tr_sel;
  d->tr_attrib    = s->tr_attrib;
  d->tr_limit     = s->tr_limit;
  d->tr_base      = s->tr_base;

  d->cpl          = s->cpl;
  d->rflags       = s->rflags;

  d->rip          = s->rip;
  d->rsp          = s->rsp;
  d->rax          = s->rax;

  d->star         = s->star;
  d->lstar        = s->lstar;
  d->cstar        = s->cstar;
  d->sfmask       = s->sfmask;
  d->kernelgsbase = s->kernelgsbase;
  d->sysenter_cs  = s->sysenter_cs;
  d->sysenter_esp = s->sysenter_esp;
  d->sysenter_eip = s->sysenter_eip;
  d->cr2          = s->cr2;

  d->dbgctl       = s->dbgctl;
  d->br_from      = s->br_from;
  d->br_to        = s->br_to;
  d->lastexcpfrom = s->lastexcpfrom;
  d->last_excpto  = s->last_excpto;
}


PRIVATE inline
void
Vm_svm::copy_control_area(Vmcb *dest, Vmcb *src)
{
  Vmcb_control_area *d = &dest->control_area;
  Vmcb_control_area *s = &src->control_area;

  d->interrupt_shadow = s->interrupt_shadow & 1;
  d->eventinj         = s->eventinj;

  Vmcb_control_area::Clean_bits clean = d->clean_bits;
  if (!clean.i())
    {
      d->intercept_rd_crX       = s->intercept_rd_crX;
      d->intercept_wr_crX       = s->intercept_wr_crX;

      d->intercept_rd_drX       = s->intercept_rd_drX;
      d->intercept_wr_drX       = s->intercept_wr_drX;

      d->intercept_exceptions   = s->intercept_exceptions;

      d->intercept_instruction0 = s->intercept_instruction0;
      d->intercept_instruction1 = s->intercept_instruction1;

      d->pause_filter_threshold = s->pause_filter_threshold;
      d->pause_filter_count     = s->pause_filter_count;

      d->tsc_offset             = s->tsc_offset;
    }

  // skip iopm_base_pa and msrpm_base_pa
  // skip asid and TLB control, those are handled in fiasco

  if (!clean.tpr())
    {
      d->int_tpr_irq = s->int_tpr_irq & 0x1ff; // 15:9 RSVD, SBZ
      d->int_ctl     = (s->int_ctl & ~0xfee0) | 0x0100;
      // 23:21 RSVD, SBZ
      // 24    always enable V_INTR_MASKING
      // 30:25 RSVD, SBZ
      // 31    disable AVIC, we do not support this

      d->int_vector  = s->int_vector;
      // no sure if this is necessary w/o V_IRQ set
    }

  if (!clean.lbr())
    d->lbr_virtualization_enable = s->lbr_virtualization_enable;
}


/* skip anything that does not change */
PRIVATE
void
Vm_svm::copy_control_area_back(Vmcb *dest, Vmcb *src)
{
  Vmcb_control_area *d = &dest->control_area;
  Vmcb_control_area *s = &src->control_area;

  d->int_tpr_irq      = s->int_tpr_irq;
  d->interrupt_shadow = s->interrupt_shadow;

  d->exitcode         = s->exitcode;
  d->exitinfo1        = s->exitinfo1;
  d->exitinfo2        = s->exitinfo2;
  d->exitintinfo      = s->exitintinfo;

  d->eventinj = s->eventinj;
  d->n_rip    = s->n_rip;
}

PRIVATE inline NOEXPORT
int
Vm_svm::do_resume_vcpu(Context *ctxt, Vcpu_state *vcpu, Vmcb *vmcb_s)
{
  assert (cpu_lock.test());

  /* these 4 must not use ldt entries */
  assert (!(Cpu::get_cs() & (1 << 2)));
  assert (!(Cpu::get_ss() & (1 << 2)));
  assert (!(Cpu::get_ds() & (1 << 2)));
  assert (!(Cpu::get_es() & (1 << 2)));

  Svm &s = Svm::cpus.current();

  // FIXME: this can be an assertion I think, however, think about MP
  if (EXPECT_FALSE(!s.svm_enabled()))
    {
      WARN("svm: not supported/enabled\n");
      return -L4_err::EInval;
    }

  // allow PAE in combination with NPT
#if 0
  // CR4.PAE must be clear
  if(vmcb_s->state_save_area.cr4 & 0x20)
    return -L4_err::EInval;
#endif

  // XXX:
  // This generates a circular dep between thread<->task, this cries for a
  // new abstraction...
  if (!(ctxt->state() & Thread_fpu_owner))
    {
      if (!static_cast<Thread*>(ctxt)->switchin_fpu())
        {
          WARN("svm: switchin_fpu failed\n");
          return -L4_err::EInval;
        }
    }

#if 0  //should never happen
  host_cr0 = Cpu::get_cr0();
  // the VMM does not currently own the fpu but wants to
  // make it available for the guest. This may happen
  // if it was descheduled between activating the fpu and
  // executing the vm_run operation
  if (!(vmcb_s->state_save_area.cr0 & 0x8) && (host_cr0 & 0x8))
    {
      WARN("svm: FPU TS\n");
      return commit_result(-L4_err::EInval);
    }
#endif

  // sanitize VMCB

  // this also handles the clean-bits for state caching
  Vmcb *kernel_vmcb_s = s.kernel_vmcb(vmcb_s);
  Vmcb_control_area::Clean_bits clean
    = kernel_vmcb_s->control_area.clean_bits;

  copy_control_area(kernel_vmcb_s, vmcb_s);
  copy_state_save_area(kernel_vmcb_s, vmcb_s);

  if (!clean.np())
    {
      kernel_vmcb_s->state_save_area.g_pat = vmcb_s->state_save_area.g_pat;
      kernel_vmcb_s->control_area.np_enable = vmcb_s->control_area.np_enable;
    }

  if (!clean.crx())
    kernel_vmcb_s->state_save_area.efer = vmcb_s->state_save_area.efer;

  if (!clean.drx())
    {
      kernel_vmcb_s->state_save_area.dr7 = vmcb_s->state_save_area.dr7;
      kernel_vmcb_s->state_save_area.dr6 = vmcb_s->state_save_area.dr6;
    }

  // barrier for the whole state save area and control area
  asm volatile ("" : "=m" (*vmcb_s));

  if (EXPECT_FALSE(!clean.np() && kernel_vmcb_s->np_enabled() && !s.has_npt()))
    {
      WARN("svm: No NPT available\n");
      return -L4_err::EInval;
    }

  // needs EFER in vmcb
  Address vm_cr3 = get_vm_cr3(kernel_vmcb_s);
  // can only fail on 64bit, will be optimized away on 32bit
  if (EXPECT_FALSE(is_64bit() && !vm_cr3))
    return -L4_err::ENomem;

  // neither EFER.LME nor EFER.LMA must be set
  if (EXPECT_FALSE(!is_64bit()
	           && (kernel_vmcb_s->state_save_area.efer & (EFER_LME | EFER_LMA))))
    {
      WARN("svm: EFER invalid %llx\n", kernel_vmcb_s->state_save_area.efer);
      return -L4_err::EInval;
    }

  // EFER.SVME must be set
  if (EXPECT_FALSE(!(kernel_vmcb_s->state_save_area.efer & 0x1000)))
    {
      WARN("svm: EFER invalid %llx\n", kernel_vmcb_s->state_save_area.efer);
      return -L4_err::EInval;
    }


  // we copy xcr0 here not in copy_state_save_area,
  // because it is written by the VMM only, never by the guest itself
  kernel_vmcb_s->state_save_area.xcr0 = vmcb_s->state_save_area.xcr0;


  // allow w access to cr0, cr2, cr3
  // allow r access to cr0, cr2, cr3, cr4

  // allow r/w access to dr[0-7]
  kernel_vmcb_s->control_area.intercept_rd_drX |= 0xff00;
  kernel_vmcb_s->control_area.intercept_wr_drX |= 0xff00;

  // enable iopm and msrpm
  kernel_vmcb_s->control_area.intercept_instruction0 |= 0x18000000;
  // intercept FERR_FREEZE and shutdown events
  kernel_vmcb_s->control_area.intercept_instruction0 |= 0xc0000000;
  // intercept INTR/NMI/SMI/INIT
  kernel_vmcb_s->control_area.intercept_instruction0 |= 0xf;
  // intercept INVD
  kernel_vmcb_s->control_area.intercept_instruction0 |= (1 << 22);
  // intercept HLT
  kernel_vmcb_s->control_area.intercept_instruction0 |= (1 << 24);
  // intercept task switch
  kernel_vmcb_s->control_area.intercept_instruction0 |= (1 << 29);
  // intercept shutdown
  kernel_vmcb_s->control_area.intercept_instruction0 |= (1 << 31);
  // intercept MONITOR/MWAIT
  kernel_vmcb_s->control_area.intercept_instruction1 |= (1 << 10) | (1 << 11);
  // intercept XSETBV
  kernel_vmcb_s->control_area.intercept_instruction1 |= (1 << 13);

  // intercept #AC and #DB
  kernel_vmcb_s->control_area.intercept_exceptions |= (1 << 1) | (1 << 17);

  // intercept virtualization related instructions
  //  vmrun interception is required by the hardware
  kernel_vmcb_s->control_area.intercept_instruction1 |= 0xff;

  Mword kernel_vmcb_pa = s.kernel_vmcb_pa();
  Unsigned64 iopm_base_pa = s.iopm_base_pa();
  Unsigned64 msrpm_base_pa = s.msrpm_base_pa();

  // XXX: handle clean bits for this
  kernel_vmcb_s->control_area.iopm_base_pa = iopm_base_pa;
  kernel_vmcb_s->control_area.msrpm_base_pa = msrpm_base_pa;

  Cpu_number ccpu = current_cpu();
  // XXX: must handle ASIDs correctly ...
  //      need to flush NP TLBs on unmaps, not on VMM request
  //      prevent VMM from using ASIDs on its own a VMM cany use
  //      multiple Vm objects instead
  if (// vmm requests flush
      (vmcb_s->control_area.tlb_ctl & 1) == 1
      // our asid is not valid or expired
      || !(s.asid_valid(asid(ccpu)->asid, asid(ccpu)->generation)))
    new_asid(ccpu, &s);

  s.flush_asids_if_needed();

  kernel_vmcb_s->control_area.guest_asid = asid(ccpu)->asid;

#if 0
  // 0/1 NP_ENABLE, 31:1 reserved SBZ
  kernel_vmcb_s->control_area.np_enable = 1;

  // 31 VALID, EVENTINJ
  kernel_vmcb_s->control_area.eventinj = 0;
#endif

  if (EXPECT_TRUE(kernel_vmcb_s->np_enabled()))
    {
      if (!clean.crx())
        {
          kernel_vmcb_s->state_save_area.cr0 = vmcb_s->state_save_area.cr0;
          kernel_vmcb_s->state_save_area.cr3 = vmcb_s->state_save_area.cr3;
          kernel_vmcb_s->state_save_area.cr4 = vmcb_s->state_save_area.cr4;
          asm volatile ("" : "=m"(vmcb_s->state_save_area));
        }
      kernel_vmcb_s->control_area.n_cr3 = vm_cr3;
    }
  else
    {
      // to do: check that the vmtask has the
      // VM property set, i.e. does not contain mappings
      // to the fiasco kernel regions or runs with PL 3

      // when we change nested paging mode or the crx regs
      // we need to reload and recheck the values
      if (!clean.crx() || !clean.np())
        {
          // enforce paging and protection enable
          kernel_vmcb_s->state_save_area.cr0 = access_once(&vmcb_s->state_save_area.cr0) | CR0_PG | CR0_PE;
          kernel_vmcb_s->state_save_area.cr4 = access_once(&vmcb_s->state_save_area.cr4);

          // FIXME: do we need more CR4 checking here ?
          // PAE for 64bit fiasco and non-PAE for 32bit fiasco
          if (is_64bit())
            kernel_vmcb_s->state_save_area.cr4 |= CR4_PAE;
          else
            kernel_vmcb_s->state_save_area.cr4 &= ~CR4_PAE;

          // fiasco uses super pages in the SPTs
          kernel_vmcb_s->state_save_area.cr4 |= CR4_PSE;
        }

      // printf("nested paging disabled, use n_cr3 as cr3\n");
      kernel_vmcb_s->state_save_area.cr3 = vm_cr3;

      // re-enforce intercepts when cache changed
      if (!clean.i())
        {
          // intercept accesses to cr0, cr3 and cr4
          kernel_vmcb_s->control_area.intercept_rd_crX = 0xfff9;
          kernel_vmcb_s->control_area.intercept_wr_crX = 0xfff9;
        }
    }


  // set MCE according to host
  kernel_vmcb_s->state_save_area.cr4 |= Cpu::get_cr4() & CR4_MCE;

  kernel_vmcb_s->control_area.clean_bits.raw = 0; // nothing clean, the safe way currently

  // to do:
  // - initialize VM_HSAVE_PA (done)
  // - supply trusted msrpm_base_pa and iopm_base_pa (done)
  // - save host state not covered by VMRUN/VMEXIT (ldt, some segments etc) (done)
  // - disable interupts (done)
  // - trigger intercepted device and timer interrupts (done, not necessary)
  // - check host CR0.TS (floating point registers) (done)

  Unsigned32 fs, gs;
  Unsigned16 tr, ldtr;

  fs = Cpu::get_fs();
  gs = Cpu::get_gs();
  tr = Cpu::get_tr();
  ldtr = Cpu::get_ldt();

  Gdt_entry tr_entry;

  Cpu &cpu = Cpu::cpus.cpu(ccpu);
  tr_entry = (*cpu.get_gdt())[tr / 8];

#if 0
  // to do: check if the nested page table walker looks
  // into the TLB. if so, global pages have to be disabled in
  // the host
  cr4 = Cpu::get_cr4();

  if (cr4 & CR4_PGE)
    // disable support for global pages as the vm task has
    // a divergent upper memory region from the regular tasks
    Cpu::set_cr4(cr4 & ~CR4_PGE);
#endif

  Unsigned64 host_xcr0 = Fpu::fpu.current().get_xcr0();

  load_guest_xcr0(host_xcr0, kernel_vmcb_s->state_save_area.xcr0);

  resume_vm_svm(kernel_vmcb_pa, &vcpu->_regs);

  load_host_xcr0(host_xcr0, kernel_vmcb_s->state_save_area.xcr0);

#if 0
  if (cr4 & CR4_PGE)
    Cpu::set_cr4(cr4);
#endif

  cpu.setup_sysenter();

  Cpu::set_ldt(ldtr);
  restore_segments(ctxt, fs, gs);

  // clear busy flag
  Gdt_entry tss_entry;

  tss_entry = (*cpu.get_gdt())[tr / 8];
  tss_entry.access &= 0xfd;
  (*cpu.get_gdt())[tr / 8] = tss_entry;

  Cpu::set_tr(tr); // TODO move under stgi in asm

  copy_state_save_area(vmcb_s, kernel_vmcb_s);
  // do not copy back EFER, MSR intercepts for EFER are always set
  // vmcb_s->state_save_area.efer = kernel_vmcb_s->state_save_area.efer;
  vmcb_s->state_save_area.g_pat = kernel_vmcb_s->state_save_area.g_pat;
  vmcb_s->state_save_area.dr7 = kernel_vmcb_s->state_save_area.dr7;
  vmcb_s->state_save_area.dr6 = kernel_vmcb_s->state_save_area.dr6;

  if (EXPECT_TRUE(kernel_vmcb_s->np_enabled()))
    {
      vmcb_s->state_save_area.cr3 = kernel_vmcb_s->state_save_area.cr3;
      vmcb_s->state_save_area.cr4 = kernel_vmcb_s->state_save_area.cr4;
    }

  copy_control_area_back(vmcb_s, kernel_vmcb_s);

  LOG_TRACE("VM-SVM", "svm", current(), Log_vm_svm_exit,
            l->exitcode = vmcb_s->control_area.exitcode;
            l->exitinfo1 = vmcb_s->control_area.exitinfo1;
            l->exitinfo2 = vmcb_s->control_area.exitinfo2;
            l->rip       = vmcb_s->state_save_area.rip;
           );

  // check for IRQ exit
  if (kernel_vmcb_s->control_area.exitcode == 0x60)
    return 1;

  force_kern_entry_vcpu_state(vcpu);
  return 0;
}

PUBLIC
int
Vm_svm::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  (void)user_mode;
  assert (user_mode);

  if (EXPECT_FALSE(!(ctxt->state(true) & Thread_ext_vcpu_enabled)))
    {
      ctxt->arch_load_vcpu_kern_state(vcpu, true);
      force_kern_entry_vcpu_state(vcpu);
      return -L4_err::EInval;
    }

  Vmcb *vmcb_s = reinterpret_cast<Vmcb*>(reinterpret_cast<char *>(vcpu) + 0x400);
  for (;;)
    {
      // in the case of disabled IRQs and a pending IRQ directly simulate an
      // external interrupt intercept
      if (   !(vcpu->_saved_state & Vcpu_state::F_irqs)
	  && (vcpu->sticky_flags & Vcpu_state::Sf_irq_pending))
	{
	  vmcb_s->control_area.exitcode = 0x60;
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          force_kern_entry_vcpu_state(vcpu);
	  return 1; // return 1 to indicate pending IRQs (IPCs)
	}

      int r = do_resume_vcpu(ctxt, vcpu, vmcb_s);

      // test for error or non-IRQ exit reason
      if (r <= 0)
        {
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          force_kern_entry_vcpu_state(vcpu);
          return r;
        }

      // check for IRQ exits and allow to handle the IRQ
      if (r == 1)
	Proc::preemption_point();

      // Check if the current context got a message delivered.
      // This is done by testing for a valid continuation.
      // When a continuation is set we have to directly
      // leave the kernel to not overwrite the vcpu-regs
      // with bogus state.
      Thread *t = nonull_static_cast<Thread*>(ctxt);

      if (t->continuation_test_and_restore())
        {
          force_kern_entry_vcpu_state(vcpu);
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          t->fast_return_to_user(vcpu->_entry_ip, vcpu->_entry_sp,
                                 t->vcpu_state().usr().get());
        }
    }
}

namespace {
static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  if (!Svm::cpu_svm_available(Cpu_number::boot_cpu()))
    return;

  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             Task::generic_factory<Vm_svm>);
}
}

// ------------------------------------------------------------------------
IMPLEMENTATION [svm && debug]:

#include "string_buffer.h"

IMPLEMENT
void
Vm_svm::Log_vm_svm_exit::print(String_buffer *buf) const
{
  buf->printf("ec=%lx ei1=%08lx ei2=%08lx rip=%08lx",
              exitcode, exitinfo1, exitinfo2, rip);
}
