IMPLEMENTATION[ia32 || amd64]:

#include "apic.h"
#include "config.h"
#include "cpu.h"
#include "io_apic.h"
#include "irq_mgr.h"
#include "koptions.h"
#include "mem_layout.h"
#include "pic.h"
#include "platform_control.h"
#include "trap_state.h"
#include "watchdog.h"

IMPLEMENT inline NEEDS["mem_layout.h"]
void
Kernel_thread::free_initcall_section()
{
  // just fill up with invalid opcodes
  for (char *i = const_cast<char *>(Mem_layout::initcall_start);
       i + 1 < const_cast<char *>(Mem_layout::initcall_end); i += 2)
    {
      // UD2
      i[0] = 0x0f;
      i[1] = 0x0b;
    }
}

PRIVATE static
void
Kernel_thread::system_resume_handler()
{
  assert(current_cpu() == Cpu_number::boot_cpu());

  Fpu::init(current_cpu(), true);

  Timer::init(current_cpu());
  Timer_tick::enable(current_cpu());
  boot_app_cpus();
}


IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  // 
  // install our slow trap handler
  //
  nested_trap_handler      = Trap_state::base_handler;
  Trap_state::base_handler = thread_handle_trap;

  Platform_control::set_system_resume_handler(
    &Kernel_thread::system_resume_handler);

  // initialize the profiling timer
  bool user_irq0 = Koptions::o()->opt(Koptions::F_irq0);

  if constexpr (int{Config::Scheduler_mode} == Config::SCHED_PIT)
    {
      if (user_irq0)
        panic("option -irq0 not possible since irq 0 is used for scheduling");
    }

  boot_app_cpus();
}

//--------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && !mp]:

PUBLIC
static inline void
Kernel_thread::boot_app_cpus()
{}


//--------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && mp]:

#include "acpi.h"
#include "mem.h"
#include "per_cpu_data.h"

EXTENSION class Kernel_thread
{
public:
  static Cpu_number find_cpu_num_by_apic_id(Apic_id apic_id);
  static bool boot_deterministic;

private:
  typedef Per_cpu_array<Apic_id> Apic_id_array;

  // store all APIC IDs found in the MADT
  // this is used by boot_ap_cpu() to determine its CPU number by looking up
  // its APIC ID in the array, the array position is equivalent to the CPU
  // number
  static Apic_id_array _cpu_num_to_apic_id;
};

Kernel_thread::Apic_id_array Kernel_thread::_cpu_num_to_apic_id;
bool Kernel_thread::boot_deterministic;

/**
 * Retrieve reserved CPU number for the given APIC ID.
 *
 * \param apic_id  Local APIC ID whose CPU number should be returned.
 *
 * \retval Cpu_number         The CPU number reserved for the provided APIC ID.
 * \retval Cpu_number::nil()  No valid CPU number for the provided APIC ID was
 *                            found.
 */
IMPLEMENT
Cpu_number
Kernel_thread::find_cpu_num_by_apic_id(Apic_id apic_id)
{
  for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
    if (_cpu_num_to_apic_id[n] == apic_id)
      return n;

  return Cpu_number::nil();
}

/**
 * Use the ACPI MADT to populate the _cpu_num_to_apic_id[] array to have a fixed
 * enumeration of APIC IDs to identify CPUs.
 */
PRIVATE static
bool
Kernel_thread::populate_cpu_num_to_apic()
{
  Acpi_madt const *madt = Io_apic::madt();
  if (!madt)
    return false;

  Apic_id boot_aid{Apic::get_id()};

  // make sure the boot CPU gets the right CPU number
  _cpu_num_to_apic_id[Cpu_number::boot_cpu()] = boot_aid;

  Cpu_number last_cpu = Cpu_number::first();

  // xAPIC: Collect all *enabled* CPUs and assign them the leading CPU numbers.
  for (auto *lapic : madt->iterate<Acpi_madt::Lapic>())
    {
      if (!(lapic->flags & 1))
        continue; // skip disabled entries

      Apic_id aid = Apic::acpi_lapic_to_apic_id(lapic->apic_id);

      if (aid == boot_aid)
        continue; // boot CPU already has a CPU number assigned

      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu; // skip logical boot CPU number

      if (last_cpu >= Config::max_num_cpus())
        break; // cannot store more CPU information

      _cpu_num_to_apic_id[last_cpu++] = aid;
    }

  // x2APIC: According to ACPI 5.2.12.12, logical processors with APIC ID values
  // less than 255 must use the Processor Local APIC structure but there is
  // hardware which has only MADT entry type LOCAL_X2AIC but no MADT entry type
  // LAPIC!
  for (auto *lx2apic : madt->iterate<Acpi_madt::Local_x2apic>())
    {
      if (!(lx2apic->flags & 1))
        continue; // skip disabled entries

      Apic_id aid{lx2apic->apic_id};

      if (aid == boot_aid)
        continue; // boot CPU already has a CPU number assigned

      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu; // skip logical boot CPU number

      if (last_cpu >= Config::max_num_cpus())
        break; // cannot store more CPU information

      _cpu_num_to_apic_id[last_cpu++] = aid;
    }

  // Collect all *disabled* CPUs and assign them the remaining CPU numbers
  // to make sure that we can boot at least the maximum number of enabled CPUs.
  // Disabled CPUs may come online later by hot plugging.
  for (auto *lapic : madt->iterate<Acpi_madt::Lapic>())
    {
      if (lapic->flags & 1)
        continue; // skip enabled entries

      Apic_id aid = Apic::acpi_lapic_to_apic_id(lapic->apic_id);

      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu; // skip logical boot CPU number

      if (last_cpu >= Config::max_num_cpus())
        break; // cannot store more CPU information

      _cpu_num_to_apic_id[last_cpu++] = aid;
    }

  for (auto *lx2apic : madt->iterate<Acpi_madt::Local_x2apic>())
    {
      if (lx2apic->flags & 1)
        continue; // skip enabled entries

      Apic_id aid{lx2apic->apic_id};

      if (aid == Apic_id{0xffffffff})
        continue; // ignore dummy entries

      if (last_cpu >= Config::max_num_cpus())
        break; // cannot store more CPU information

      _cpu_num_to_apic_id[last_cpu++] = aid;
   }

  return last_cpu > Cpu_number::first();
}

PUBLIC
static void
Kernel_thread::boot_app_cpus()
{
  // where to start the APs for detection of the APIC-IDs
  extern char _tramp_mp_entry[];

  // feature enabling flags (esp. cache enabled flag and paging enabled flag)
  extern volatile Mword _tramp_mp_startup_cr0;

  // feature enabling flags (esp. needed for big pages)
  extern volatile Mword _tramp_mp_startup_cr4;

  // physical address of the page table directory to use
  extern volatile Address _realmode_startup_pdbr;

  // pseudo descriptor for the gdt to load
  extern Pseudo_descriptor _tramp_mp_startup_gdt_pdesc;

  Address tramp_page;

  _realmode_startup_pdbr = Kmem::get_realmode_startup_pdbr();
  _tramp_mp_startup_cr4 = Cpu::get_cr4();
  _tramp_mp_startup_cr0 = Cpu::get_cr0();
  _tramp_mp_startup_gdt_pdesc = Kmem::get_realmode_startup_gdt_pdesc();

  Mem::barrier();

  if (populate_cpu_num_to_apic())
    boot_deterministic = true;

  printf("MP: detecting APs...\n");

  // broadcast an AP startup via the APIC (let run the self-registration code)
  tramp_page = reinterpret_cast<Address>(&_tramp_mp_entry[0]);

  // Send IPI sequence to startup the APs
  Apic::mp_startup(Apic_id{0} /*ignored*/, true, tramp_page);
}

//--------------------------------------------------------------------------
IMPLEMENTATION[(ia32 || amd64) && !tickless_idle]:

#include "processor.h"

IMPLEMENT_OVERRIDE inline NEEDS["processor.h"]
void
Kernel_thread::idle_op()
{
  if (Config::hlt_works_ok)
    Proc::halt();     // stop the CPU, waiting for an interrupt
  else
    Proc::pause();
}
