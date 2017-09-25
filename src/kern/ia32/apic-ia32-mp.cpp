INTERFACE [mp]:

EXTENSION class Apic
{
public:
  typedef enum
    {
      APIC_IPI_NOSHRT = 0x00000000,
      APIC_IPI_SELF   = 0x00040000,
      APIC_IPI_ALL    = 0x00080000,
      APIC_IPI_OTHERS = 0x000c0000,
      APIC_IPI_DSTMSK = 0x000c0000
    } Apic_ipi_dest;

  typedef enum
    {
      APIC_IPI_FIXED  = 0x00000000,
      APIC_IPI_NMI    = 0x00000400,
      APIC_IPI_INIT   = 0x00000500,
      APIC_IPI_STRTUP = 0x00000600
    } Apic_ipi_mode;

private:
  enum
    {
      APIC_ICR	= 0x300,
      APIC_ICR2	= 0x310,
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
  reg_write(APIC_lvt0, 0x0001003f);
  reg_write(APIC_lvt1, 0x0001003f);
}

PUBLIC static inline
bool
Apic::mp_ipi_idle()
{
  return ((reg_read(APIC_ICR) & 0x00001000) == 0);
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

PUBLIC static inline NEEDS [<cassert>]
void
Apic::mp_send_ipi(Unsigned32 dest, Unsigned32 vect,
                  Unsigned32 mode = APIC_IPI_FIXED)
{
  Unsigned32 tmp_val;

  assert((dest & 0x00f3ffff) == 0);
  assert(vect <= 0xff);

  while (!mp_ipi_idle())
    Proc::pause();

  // Set destination for no-shorthand destination type
  if ((dest & APIC_IPI_DSTMSK) == APIC_IPI_NOSHRT)
    {
      tmp_val  = reg_read(APIC_ICR2);
      tmp_val &= 0x00ffffff;
      tmp_val |= dest & 0xff000000;
      reg_write(APIC_ICR2, tmp_val);
    }

  // send the interrupt vector to the destination...
  tmp_val  = reg_read(APIC_ICR);
  tmp_val &= 0xfff32000;
  tmp_val |= (dest & 0x000c0000) |
             (       0x00004000) | // phys proc num, edge triggered, assert
             (mode & 0x00000700) |
             (vect & 0x000000ff);
  reg_write(APIC_ICR, tmp_val);
}

PUBLIC static inline
void
Apic::mp_ipi_ack()
{
  reg_write(APIC_eoi, 0);
}


PUBLIC static
void
Apic::init_ap()
{
  dump_info();
  // set some interrupt vectors to appropriate values
  init_lvt();

  // initialize APIC_spiv register
  init_spiv();

  // initialize task-priority register
  init_tpr();

  disable_external_ints();

  // get timer going on this CPU
  timer_set_divisor(1);
  enable_errors();
}

PUBLIC static
void
Apic::mp_startup(Cpu const *current_cpu, Unsigned32 dest, Address tramp_page)
{
  assert((tramp_page & 0xfff00fff) == 0);

  // XXX: should check for the apic version what to do exactly
  // XXX: should check for some errors after sending ipi

  // Send INIT IPI
  mp_send_ipi(dest, 0, APIC_IPI_INIT);

  delay(current_cpu, 200);

  // delay for 10ms (=10,000us)
  if (!mp_ipi_idle_timeout(current_cpu, 10000))
    return;

  // Send STARTUP IPI
  mp_send_ipi(dest, tramp_page >> 12, APIC_IPI_STRTUP);

  // delay for 200us
  if (!mp_ipi_idle_timeout(current_cpu, 200))
    return;

  // Send STARTUP IPI
  mp_send_ipi(dest, tramp_page >> 12, APIC_IPI_STRTUP);

  // delay for 200us
  if (!mp_ipi_idle_timeout(current_cpu, 200))
    return;
}
