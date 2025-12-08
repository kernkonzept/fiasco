INTERFACE [mp]:

EXTENSION class Apic
{
public:
  enum class Ipi_dest_shrt : Unsigned8
  {
    Noshrt = 0b00,
    Self   = 0b01,
    All    = 0b10,
    Others = 0b11,
  };

  enum class Ipi_delivery_mode : Unsigned8
  {
    Fixed   = 0b000,
    Nmi     = 0b100,
    Init    = 0b101,
    Startup = 0b110,
  };
};


IMPLEMENTATION[mp]:

#include <cassert>
#include "cpu.h"
#include "processor.h"

PUBLIC static inline
void
Apic::disable_external_ints()
{
  reg_write(Reg::Lvt0, 0x0001003f);
  reg_write(Reg::Lvt1, 0x0001003f);
}

PUBLIC static inline
bool
Apic::mp_ipi_idle()
{
  if (use_x2apic())
    return true;
  else
    return ((reg_read(Reg::Icr) & 0x00001000) == 0);
}

PRIVATE static inline
bool
Apic::mp_ipi_idle_timeout(Cpu const *c, Unsigned32 wait)
{
  Unsigned64 wait_till = c->time_us() + wait;
  while (!mp_ipi_idle() && c->time_us() < wait_till)
    Proc::pause();
  return mp_ipi_idle();
}

PRIVATE static inline
void
Apic::delay(Cpu const *c, Unsigned32 wait)
{
  Unsigned64 wait_till = c->time_us() + wait;
  while (c->time_us() < wait_till)
    Proc::pause();
}

PUBLIC static inline
void
Apic::mp_send_ipi(Ipi_dest_shrt dest_shrt, Apic_id dest,
                  Ipi_delivery_mode delivery_mode, Unsigned8 vector)
{
  while (!mp_ipi_idle())
    Proc::pause();

  // Do NOT consider INIT Level De-assert (Delivery Mode: INIT, Level: 1/Assert,
  // Trigger Mode: 1/Level). Since Pentium 4, this mode is no longer supported
  // anyway.
  Unsigned32 lower_icr =   static_cast<Unsigned32>(dest_shrt) << 18
                         | 0U << 15 // Trigger Mode: Edge
                         | 1U << 14 // Level: Assert, including INIT
                         | 0U << 11 // Destination Mode: Physical
                         | static_cast<Unsigned32>(delivery_mode) << 8
                         | vector;

  if (use_x2apic())
    {
      asm volatile ("mfence; lfence"); // enforce serializing as in xAPIC mode
      reg_write64(Reg::Icr, lower_icr, cxx::int_value<Apic_id>(dest));
    }
  else
    {
      if (dest_shrt == Ipi_dest_shrt::Noshrt)
        reg_write(Reg::Icr2, cxx::int_value<Apic_id>(dest));
      reg_write(Reg::Icr, lower_icr);
    }
}

PUBLIC static inline
void
Apic::mp_ipi_ack()
{
  reg_write(Reg::Eoi, 0);
}

PUBLIC static
void
Apic::init_ap()
{
  init_lvt();
  init_spiv();
  init_tpr();

  disable_external_ints();

  timer_set_divisor(1);
  enable_errors();
}

PUBLIC static
int
Apic::mp_startup(Apic_id dest, bool bcast, Address tramp_page)
{
  assert(current_cpu() == Cpu_number::boot_cpu());
  assert((tramp_page & 0xfff00fff) == 0);

  Ipi_dest_shrt dest_shrt = bcast ? Ipi_dest_shrt::Others : Ipi_dest_shrt::Noshrt;

  Cpu const *boot_cpu = Cpu::boot_cpu();

  // XXX: should check for some errors after sending ipi

  reg_write(Reg::Esr, 0);

  // Send INIT IPI
  mp_send_ipi(dest_shrt, dest, Ipi_delivery_mode::Init, 0);

  delay(boot_cpu, 200);

  // delay for 10ms (=10,000us)
  if (!mp_ipi_idle_timeout(boot_cpu, 10000))
    return 1;

  // Send STARTUP IPI
  mp_send_ipi(dest_shrt, dest, Ipi_delivery_mode::Startup, tramp_page >> 12);

  // delay for 200us
  if (!mp_ipi_idle_timeout(boot_cpu, 200))
    return 2;

  // Send STARTUP IPI
  mp_send_ipi(dest_shrt, dest, Ipi_delivery_mode::Startup, tramp_page >> 12);

  // delay for 200us
  if (!mp_ipi_idle_timeout(boot_cpu, 200))
    return 3;

  unsigned esr = reg_read(Reg::Esr);
  if (esr)
    printf("APIC status: %x\n", esr);

  return 0;
}
