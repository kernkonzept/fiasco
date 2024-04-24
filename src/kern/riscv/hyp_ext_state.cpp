INTERFACE [riscv && cpu_virt]:

#include "cpu.h"

class Hyp_ext_state
{
public:
  enum Hsxlen : Unsigned8
  {
    Hsxlen_32  = 1,
    Hsxlen_64  = 2,
    Hsxlen_128 = 3,
  };

  enum Rfnc : Unsigned8
  {
    Rfnc_none            = 0,
    Rfnc_fence_i         = 1,
    Rfnc_sfence_vma      = 2,
    Rfnc_sfence_vma_asid = 3,
  };

  Mword hedeleg;
  Mword hideleg;

  Mword hvip;
  Mword hip; // read-only
  Mword hie;

  Unsigned64 htimedelta;

  Mword htval;
  Mword htinst;

  Mword vsstatus;
  Mword vstvec;
  Mword vsscratch;
  Mword vsepc;
  Mword vscause;
  Mword vstval;
  Mword vsatp;
  Unsigned64 vstimecmp; // ignored if Sstc extension is not available

  // Indicates that a hypervisor load/store instruction failed. VMM is
  // responsible for resetting this value before executing a hypervisor/load
  // store instruction.
  Unsigned8 hlsi_failed;

  Rfnc remote_fence;
  Mword remote_fence_hart_mask;
  Mword remote_fence_start_addr;
  Mword remote_fence_size;
  Mword remote_fence_asid;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt]:

#include "mem_unit.h"
#include "sbi.h"

PUBLIC
void
Hyp_ext_state::load()
{
  if(reinterpret_cast<Mword>(this) == 0x400u)
    WARN("Hyp_ext_state::load(): No vcpu state allocated!\n");

  csr_write(hedeleg, hedeleg & Cpu::Hedeleg_mask);
  csr_write(hideleg, hideleg);

  csr_write(hvip, hvip);
  csr_write(hie, hie);

  csr_write64(htimedelta, htimedelta);

  csr_write(vsstatus, vsstatus);
  csr_write(vstvec, vstvec);
  csr_write(vsscratch, vsscratch);
  csr_write(vsepc, vsepc);
  csr_write(vscause, vscause);
  csr_write(vstval, vstval);
  csr_write(vsatp, vsatp);

  if (Cpu::has_isa_ext(Cpu::Isa_ext_sstc))
    csr_write64(vstimecmp, vstimecmp);
}

PUBLIC
void
Hyp_ext_state::save()
{
  hedeleg = csr_read(hedeleg);
  hideleg = csr_read(hideleg);

  hvip = csr_read(hvip);
  hip = csr_read(hip);
  hie = csr_read(hie);

  htimedelta = csr_read64(htimedelta);

  htval = csr_read(htval);
  // Reset htinst, as we use it to detect trapping HLV/HSV instructions
  htinst = csr_read_write(htinst, 0);

  vsstatus = csr_read(vsstatus);
  vstvec = csr_read(vstvec);
  vsscratch = csr_read(vsscratch);
  vsepc = csr_read(vsepc);
  vscause = csr_read(vscause);
  vstval = csr_read(vstval);
  vsatp = csr_read(vsatp);

  if (Cpu::has_isa_ext(Cpu::Isa_ext_sstc))
    vstimecmp = csr_read64(vstimecmp);
}

PUBLIC
void
Hyp_ext_state::load_vmm()
{
  // For hypervisor load-store instructions to work, the correct vsstatus and
  // vsatp registers have to be loaded.
  csr_write(vsstatus, vsstatus);
  csr_write(vsatp, vsatp);
}

PUBLIC
void
Hyp_ext_state::process_remote_fence()
{
  if (EXPECT_TRUE(remote_fence == Rfnc_none))
    return;

  Mword cpu_mask = remote_fence_hart_mask;
  if (!cpu_mask)
    return;

  // Translate cpu mask into hart mask
  Hart_mask hart_mask;
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    {
      if ((cpu_mask & (1UL << Cpu_number::val(i))) && Cpu::online(i))
        hart_mask.set(Cpu::cpus.cpu(i).phys_id());
    }

  if (hart_mask.empty())
    return;

  switch (remote_fence)
    {
      case Rfnc_fence_i:
        Sbi::Rfnc::remote_fence_i(hart_mask, 0);
        break;
      case Rfnc_sfence_vma:
        Sbi::Rfnc::remote_hfence_vvma(
          hart_mask, 0, remote_fence_start_addr, remote_fence_size);
        break;
      case Rfnc_sfence_vma_asid:
        Sbi::Rfnc::remote_hfence_vvma_asid(
          hart_mask, 0, remote_fence_start_addr,
          remote_fence_size, remote_fence_asid);
        break;
      default:
        break;
    }

  // We processed the pending remote fence
  remote_fence = Rfnc_none;
}
