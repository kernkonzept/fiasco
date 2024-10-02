INTERFACE:

#include "types.h"

EXTENSION class Platform_control
{
public:
  static void boot_secondary_harts();
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "processor.h"
#include "sbi.h"

IMPLEMENT_OVERRIDE
void
Platform_control::system_off()
{
  Sbi::shutdown();
}

PUBLIC [[noreturn]] static
void
Platform_control::stop_cpu()
{
  if (Sbi::Hsm::probe())
    Sbi::Hsm::hart_stop();

  for (;;)
    Proc::halt();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

IMPLEMENT_DEFAULT
void
Platform_control::boot_secondary_harts()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "kip.h"
#include "mem.h"
#include "processor.h"

IMPLEMENT_DEFAULT
void
Platform_control::boot_secondary_harts()
{
  Mem::mp_mb();

  // Boot secondary harts
  extern Mword _tramp_mp_main_hart_done;
  _tramp_mp_main_hart_done = 1;

  extern char _start_hsm[];
  // This pointer proxy is needed, because in all of RISC-V's code models
  // statically defined symbols must lie within a single 2 GiB address range.
  // For _start_hsm this condition does not hold, as it is put into the
  // bootstrap.text section. Therefore, linking of code that directly accesses
  // _start_hsm would fail with a "relocation truncated to fit" error.
  static Mword proxy_start_hsm __attribute__((used)) =
    reinterpret_cast<Mword>(_start_hsm);

  // Request start of secondary harts
  if (Sbi::Hsm::probe())
    {
      auto const & arch_info = Kip::k()->platform_info.arch;
      Unsigned32 boot_hart_id = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
      for (unsigned i = 0; i < arch_info.hart_num; i++)
        {
          Unsigned32 hart_id = arch_info.hart_ids[i];
          if (hart_id != boot_hart_id)
            {
              Sbi::Hsm::hart_start(hart_id, proxy_start_hsm, 0);
            }
        }
    }
}
