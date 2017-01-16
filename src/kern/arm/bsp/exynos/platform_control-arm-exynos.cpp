INTERFACE [arm && pf_exynos]:

#include "types.h"
#include "io.h"
#include "mem_layout.h"
#include "mmio_register_block.h"

EXTENSION class Platform_control
{
public:

  class Pmu : public Mmio_register_block
  {
  public:
    explicit Pmu(Address virt) : Mmio_register_block(virt) {}
    enum Reg
    {
      Config      = 0,
      Status      = 4,
      Option      = 8,
      Core_offset = 0x80,

      ARM_COMMON_OPTION      = 0x2408,
   };

    static Mword core_pwr_reg(Cpu_phys_id cpu, unsigned func)
    { return 0x2000 + Core_offset * cxx::int_value<Cpu_phys_id>(cpu) + func; }

    Mword read(unsigned reg) const
    { return Mmio_register_block::read<Mword>(reg); }

    void write(Mword val, unsigned reg) const
    { Mmio_register_block::write(val, reg); }

    Mword core_read(Cpu_phys_id cpu, unsigned reg) const
    { return Mmio_register_block::read<Mword>(core_pwr_reg(cpu, reg)); }

    void core_write(Mword val, Cpu_phys_id cpu, unsigned reg) const
    { Mmio_register_block::write(val, core_pwr_reg(cpu, reg)); }
  };

  enum Power_down_mode
  {
    Aftr, Lpa, Dstop, Sleep,

    Pwr_down_mode = Aftr,
  };

  enum Pmu_regs
  {
    Pmu_core_local_power_enable = 3,
  };

  static Static_object<Pmu> pmu;
};

//--------------------------------------------------------------------------
INTERFACE [arm && pf_exynos && (exynos_extgic || pf_exynos5) && cpu_suspend]:

#include "per_cpu_data.h"

EXTENSION class Platform_control
{
public:
  static Per_cpu<bool> _suspend_allowed;
};

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && !mp]:

PRIVATE static
int
Platform_control::power_up_core(Cpu_phys_id)
{
  return -L4_err::ENodev;
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos]:

#include "per_cpu_data.h"

Static_object<Platform_control::Pmu> Platform_control::pmu;

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && mp && cpu_suspend]:

#include "poll_timeout_kclock.h"

PRIVATE static
int
Platform_control::power_up_core(Cpu_phys_id cpu)
{
  // CPU already powered up?
  if ((pmu->core_read(cpu, Pmu::Status) & Pmu_core_local_power_enable) != 0)
    return 0;

  pmu->core_write(Pmu_core_local_power_enable, cpu, Pmu::Config);

  Lock_guard<Cpu_lock, Lock_guard_inverse_policy> cpu_lock_guard(&cpu_lock);

  Poll_timeout_kclock pt(10000);
  while (pt.test((pmu->core_read(cpu, Pmu::Status)
                  & Pmu_core_local_power_enable)
                 != Pmu_core_local_power_enable))
      ;

  return pt.timed_out() ? -L4_err::ENodev : 0;
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && mp && !cpu_suspend]:

PRIVATE static
int
Platform_control::power_up_core(Cpu_phys_id)
{
  return 0;
}


//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos]:

#include "kmem.h"
#include "mem_unit.h"
#include "outer_cache.h"

PRIVATE static
void
Platform_control::write_phys_mem_coherent(Mword addr_p, Mword value)
{
  Mword addr_v = Kmem::mmio_remap(addr_p);
  Io::write<Mword>(value, addr_v);
  Mem_unit::flush_dcache((void *)addr_v, (void *)(addr_v + sizeof(value)));
  Outer_cache::flush(addr_p);
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && mp && arm_em_ns && arm_secmonif_mc]:

PRIVATE static
void
Platform_control::cpuboot(Mword startup_vector, Cpu_phys_id cpu)
{
  unsigned long b = Mem_layout::Sysram_phys_base;
  if (Platform::is_5410())
    b += 0x5301c;
  else
    b += Platform::is_4210() ? 0x1f01c : 0x2f01c;

  if (Platform::is_4412())
    b += cxx::int_value<Cpu_phys_id>(cpu) * 4;

  write_phys_mem_coherent(b, startup_vector);
  Exynos_smc::call(Exynos_smc::Cpu1boot, cxx::int_value<Cpu_phys_id>(cpu));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [pf_exynos && mp && (!arm_em_ns || arm_secmonif_none)]:

#include "mem_layout.h"
#include "platform.h"

PRIVATE static
void
Platform_control::cpuboot(Mword startup_vector, Cpu_phys_id cpu)
{
 unsigned long b = Mem_layout::Sysram_phys_base;

  if (Platform::is_4210() && Platform::subrev() == 0x11)
    b = Mem_layout::Pmu_phys_base + 0x814;
  else if (Platform::is_4210() && Platform::subrev() == 0)
    b += 0x5000;

  if (Platform::is_4412())
    b += cxx::int_value<Cpu_phys_id>(cpu) * 4;

  write_phys_mem_coherent(b, startup_vector);
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && mp]:

#include "ipi.h"
#include "kmem.h"
#include "mem_unit.h"
#include "outer_cache.h"
#include "platform.h"
#include "smc.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_reset_vector)
{
  assert(current_cpu() == Cpu_number::boot_cpu());

  if (Platform::is_4412() || Platform::is_5410())
    {
      for (Cpu_phys_id i = Cpu_phys_id(1);
           i < Cpu_phys_id(4) && i < Cpu_phys_id(Config::Max_num_cpus);
           ++i)
        {
          power_up_core(i);
          if (Platform::is_4412())
            cpuboot(phys_reset_vector, i);
          Ipi::send(Ipi::Global_request, Cpu_number::boot_cpu(), i);
        }

      return;
    }

  Cpu_phys_id const second = Cpu_phys_id(1);
  power_up_core(second);
  cpuboot(phys_reset_vector, second);
  Ipi::send(Ipi::Global_request, Cpu_number::boot_cpu(), second);
}


//--------------------------------------------------------------------------
IMPLEMENTATION [arm
                && ((pf_exynos && !exynos_extgic && !pf_exynos5 && cpu_suspend)
                    || !cpu_suspend)]:

#include "l4_types.h"

PUBLIC static inline
bool
Platform_control::cpu_suspend_allowed(Cpu_number)
{ return false; }

PUBLIC static
int
Platform_control::do_core_n_off(Cpu_number)
{
  return -L4_err::EBusy;
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && mp && (exynos_extgic || pf_exynos5)
                && cpu_suspend]:

#include <cstdio>
#include "cpu.h"
#include "io.h"
#include "ipi.h"
#include "kmem.h"
#include "pic.h"
#include "processor.h"
#include "smc.h"

IMPLEMENT_OVERRIDE inline
bool
Platform_control::cpu_shutdown_available()
{ return true; }

PRIVATE static inline
int
Platform_control::resume_cpu(Cpu_number cpu)
{
  Cpu_phys_id const pcpu = Cpu::cpus.cpu(cpu).phys_id();
  int r;
  if ((r = power_up_core(pcpu)))
    return r;

  set_suspend_state(cpu, false);
  extern char _tramp_mp_entry[];
  cpuboot(Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry), pcpu);
  Ipi::send(Ipi::Global_request, current_cpu(), pcpu);

  return 0;
}

PRIVATE static inline
int
Platform_control::suspend_cpu(Cpu_number cpu)
{
  set_suspend_state(cpu, true);
  Ipi::send(Ipi::Global_request, current_cpu(), cpu);

  return 0;
}

IMPLEMENT_OVERRIDE
int
Platform_control::cpu_allow_shutdown(Cpu_number cpu, bool allow)
{
  if (allow && !Cpu::online(cpu))
    suspend_cpu(cpu);
  else if (!allow && Cpu::online(cpu))
    resume_cpu(cpu);
  else
    return -L4_err::ENodev;

  return 0;
}


//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && cpu_suspend && arm_em_ns]:

PRIVATE static inline
void
Platform_control::do_sleep()
{
  // FIXME: I miss: Exynos_smc::cpusleep();
  asm volatile("dsb; wfi" : : : "memory", "cc");
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && cpu_suspend && !arm_em_ns]:

PRIVATE static inline
void
Platform_control::do_sleep()
{
  asm volatile("dsb; wfi" : : : "memory", "cc");
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos && (exynos_extgic || pf_exynos5) && cpu_suspend]:

#include <cassert>
#include <cstdio>
#include "cpu.h"
#include "io.h"
#include "kmem.h"
#include "pic.h"
#include "processor.h"

DEFINE_PER_CPU Per_cpu<bool> Platform_control::_suspend_allowed;

IMPLEMENT_OVERRIDE
void
Platform_control::init(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      assert (!pmu->get_mmio_base());
      pmu.construct(Kmem::mmio_remap(Mem_layout::Pmu_phys_base));

      for (Cpu_phys_id i = Cpu_phys_id(0);
           i < Cpu_phys_id(2);
           ++i)
        pmu->core_write((pmu->core_read(i, Pmu::Option) & ~(1 << 0)) | (1 << 1), i, Pmu::Option);

      pmu->write(2, Pmu::ARM_COMMON_OPTION);
    }
}

PUBLIC static
void
Platform_control::set_suspend_state(Cpu_number cpu, bool state)
{
  _suspend_allowed.cpu(cpu) = state;
}

PUBLIC static
bool
Platform_control::cpu_suspend_allowed(Cpu_number cpu)
{
  return _suspend_allowed.cpu(cpu);
}

PUBLIC static
void
Platform_control::do_print_cpu_info(Cpu_phys_id cpu)
{
  printf("fiasco: core%d: %lx/%lx/%lx\n", cxx::int_value<Cpu_phys_id>(cpu),
         pmu->core_read(cpu, Pmu::Config),
         pmu->core_read(cpu, Pmu::Status),
         pmu->core_read(cpu, Pmu::Option));
}

PUBLIC static
int
Platform_control::do_core_n_off(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    return -L4_err::EBusy;

  Cpu_phys_id const phys_cpu = Cpu::cpus.cpu(cpu).phys_id();

  do_print_cpu_info(phys_cpu);

  assert(cpu_lock.test()); // required for wfi

  pmu->core_write(0, phys_cpu, Pmu::Config);

  Mem_unit::flush_cache();
  Mem_unit::tlb_flush();
  Mem_unit::kernel_tlb_flush();

  Cpu::disable_smp();
  Cpu::disable_dcache();

  do_sleep();

  // we only reach here if the wfi was not done due to a pending event

  Cpu::enable_dcache();
  Cpu::enable_smp();

  // todo: the timer irq needs a proper cpu setting here too
  // (save + restore state)
  Pic::reinit(cpu);

  do_print_cpu_info(phys_cpu);

  return 0;
}
