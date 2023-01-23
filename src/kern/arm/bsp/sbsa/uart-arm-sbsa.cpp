IMPLEMENTATION:

#include "koptions.h"

enum {
  Acpi_spcr_nsc16550 = 0,
  Acpi_spcr_arm_pl011 = 3,
  Acpi_spcr_arm_sbsa_32bit = 13,
  Acpi_spcr_arm_sbsa = 14,
};


IMPLEMENT_OVERRIDE
void
Uart::bsp_init()
{
  set_uart_instance(Koptions::o()->uart.variant == Acpi_spcr_nsc16550 ? 1 : 0);
}
