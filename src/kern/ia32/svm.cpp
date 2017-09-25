INTERFACE:

class Svm
{
};

//-----------------------------------------------------------------------------
INTERFACE[svm]:

#include "per_cpu_data.h"
#include "virt.h"
#include "cpu_lock.h"
#include "pm.h"

EXTENSION class Svm : public Pm_object
{
public:
  static Per_cpu<Svm> cpus;

  enum Msr_perms
  {
    Msr_intercept = 3,
    Msr_ro        = 2,
    Msr_wo        = 1,
    Msr_rw        = 0,
  };

private:
  Vmcb const *_last_user_vmcb;
  Unsigned32 _next_asid;
  Unsigned32 _global_asid_generation;
  bool _flush_all_asids;

  /* read mostly below */
  Unsigned32 _max_asid;
  bool _svm_enabled;
  bool _has_npt;
  void *_vm_hsave_area;
  void *_iopm;
  void *_msrpm;
  Unsigned64 _iopm_base_pa;
  Unsigned64 _msrpm_base_pa;
  Vmcb *_kernel_vmcb;
  Address _kernel_vmcb_pa;
};

//-----------------------------------------------------------------------------
INTERFACE [svm && ia32]:

EXTENSION class Svm
{
public:
  enum { Gpregs_words = 10 };
};

//-----------------------------------------------------------------------------
INTERFACE [svm && amd64]:

EXTENSION class Svm
{
public:
  enum { Gpregs_words = 18 };
};

// -----------------------------------------------------------------------
IMPLEMENTATION[svm]:

#include "cpu.h"
#include "kmem.h"
#include "l4_types.h"
#include "warn.h"
#include <cstring>

DEFINE_PER_CPU_LATE Per_cpu<Svm> Svm::cpus(Per_cpu_data::Cpu_num);

PUBLIC
void
Svm::pm_on_suspend(Cpu_number)
{
  // FIXME: Handle VMCB caching stuff iff enabled
}

PUBLIC
void
Svm::pm_on_resume(Cpu_number)
{
  Unsigned64 efer = Cpu::rdmsr(MSR_EFER);
  efer |= 1 << 12;
  Cpu::wrmsr(efer, MSR_EFER);
  Unsigned64 vm_hsave_pa = Kmem::virt_to_phys(_vm_hsave_area);
  Cpu::wrmsr(vm_hsave_pa, MSR_VM_HSAVE_PA);
  _last_user_vmcb = 0;
}

PUBLIC static inline NEEDS["cpu.h"]
bool
Svm::cpu_svm_available(Cpu_number cpu)
{
  Cpu &c = Cpu::cpus.cpu(cpu);

  if (!c.online() || !c.svm())
    return false;

  Unsigned64 vmcr;
  vmcr = c.rdmsr(MSR_VM_CR);
  if (vmcr & (1 << 4)) // VM_CR.SVMDIS
    return false;
  return true;
}

PUBLIC
Svm::Svm(Cpu_number cpu)
{
  Cpu &c = Cpu::cpus.cpu(cpu);
  _last_user_vmcb = 0;
  _svm_enabled = false;
  _next_asid = 1;
  _global_asid_generation = 0;
  _max_asid = 0;
  _flush_all_asids = true;
  _has_npt = false;

  if (!cpu_svm_available(cpu))
    return;

  printf("SVM: enabled\n");

  Unsigned64 efer;
  efer = c.rdmsr(MSR_EFER);
  efer |= 1 << 12;
  c.wrmsr(efer, MSR_EFER);

  Unsigned32 eax, ebx, ecx, edx;
  c.cpuid (0x8000000a, &eax, &ebx, &ecx, &edx);
  if (edx & 1)
    {
      printf("SVM: nested paging supported\n");
      _has_npt = true;
    }
  printf("SVM: NASID: %u\n", ebx);
  _max_asid = ebx - 1;

  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  assert(_max_asid > 0);

  enum
  {
    Vmcb_size   = 0x1000,
    Io_pm_size  = 0x3000,
    Msr_pm_size = 0x2000,
    State_save_area_size = 0x1000,
  };

  /* 16kB IO permission map and Vmcb (16kB are good for the buddy allocator)*/
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_iopm = Kmem_alloc::allocator()->unaligned_alloc(Io_pm_size + Vmcb_size));
  _iopm_base_pa = Kmem::virt_to_phys(_iopm);
  _kernel_vmcb = (Vmcb*)((char*)_iopm + Io_pm_size);
  _kernel_vmcb_pa = Kmem::virt_to_phys(_kernel_vmcb);
  _svm_enabled = true;

  /* disbale all ports */
  memset(_iopm, ~0, Io_pm_size);

  /* clean out vmcb */
  memset(_kernel_vmcb, 0, Vmcb_size);

  /* 8kB MSR permission map */
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_msrpm = Kmem_alloc::allocator()->unaligned_alloc(Msr_pm_size));
  _msrpm_base_pa = Kmem::virt_to_phys(_msrpm);
  memset(_msrpm, ~0, Msr_pm_size);

  // allow the sysenter MSRs for the guests
  set_msr_perm(MSR_SYSENTER_CS, Msr_rw);
  set_msr_perm(MSR_SYSENTER_EIP, Msr_rw);
  set_msr_perm(MSR_SYSENTER_ESP, Msr_rw);
  set_msr_perm(MSR_GS_BASE, Msr_rw);
  set_msr_perm(MSR_FS_BASE, Msr_rw);
  set_msr_perm(MSR_KERNEL_GS_BASE, Msr_rw);
  set_msr_perm(MSR_CSTAR, Msr_rw);
  set_msr_perm(MSR_LSTAR, Msr_rw);
  set_msr_perm(MSR_SFMASK, Msr_rw);

  /* 4kB Host state-safe area */
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_vm_hsave_area = Kmem_alloc::allocator()->unaligned_alloc(State_save_area_size));
  Unsigned64 vm_hsave_pa = Kmem::virt_to_phys(_vm_hsave_area);

  c.wrmsr(vm_hsave_pa, MSR_VM_HSAVE_PA);
  register_pm(cpu);
}

PUBLIC
void
Svm::set_msr_perm(Unsigned32 msr, Msr_perms perms)
{
  unsigned offs;
  if (msr <= 0x1fff)
    offs = 0;
  else if (0xc0000000 <= msr && msr <= 0xc0001fff)
    offs = 0x800;
  else if (0xc0010000 <= msr && msr <= 0xc0011fff)
    offs = 0x1000;
  else
    {
      WARN("Illegal MSR %x\n", msr);
      return;
    }

  msr &= 0x1fff;
  offs += msr / 4;

  unsigned char *pm = (unsigned char *)_msrpm;

  unsigned shift = (msr & 3) * 2;
  pm[offs] = (pm[offs] & ~(3 << shift)) | ((unsigned char)perms << shift);
}

PUBLIC
Unsigned64
Svm::iopm_base_pa()
{ return _iopm_base_pa; }

PUBLIC
Unsigned64
Svm::msrpm_base_pa()
{ return _msrpm_base_pa; }

/**
 * \pre user_vmcb must be a uniqe address across all address spaces
 *      (e.g., a kernel KU-mem address)
 */
PUBLIC inline
Vmcb *
Svm::kernel_vmcb(Vmcb const *user_vmcb)
{
  if (user_vmcb != _last_user_vmcb)
    {
      _kernel_vmcb->control_area.clean_bits.raw = 0;
      _last_user_vmcb = user_vmcb;
    }
  else
    _kernel_vmcb->control_area.clean_bits = access_once(&user_vmcb->control_area.clean_bits);

  return _kernel_vmcb;
}

PUBLIC
Address
Svm::kernel_vmcb_pa()
{ return _kernel_vmcb_pa; }

PUBLIC
bool
Svm::svm_enabled()
{ return _svm_enabled; }

PUBLIC
bool
Svm::has_npt()
{ return _has_npt; }

PUBLIC
bool
Svm::asid_valid(Unsigned32 asid, Unsigned32 generation)
{
  return ((asid > 0) &&
          (asid <= _max_asid) &&
          (generation <= _global_asid_generation));
}

PUBLIC inline
void
Svm::flush_asids_if_needed()
{
  if (EXPECT_TRUE(!_flush_all_asids))
    return;

  _flush_all_asids = false;
  _kernel_vmcb->control_area.tlb_ctl |= 1;
}

PUBLIC inline
Unsigned32
Svm::global_asid_generation() const
{ return _global_asid_generation; }

PUBLIC
Unsigned32
Svm::next_asid()
{
  assert (cpu_lock.test());
  if (_next_asid > _max_asid)
    {
      _global_asid_generation++;
      _next_asid = 1;
      // FIXME: must not crash on an overrun
      assert (_global_asid_generation < ~0U);
      _flush_all_asids = true;
    }
  return _next_asid++;
}
