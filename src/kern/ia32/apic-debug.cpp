IMPLEMENTATION[debug]:

#include <cstdio>
#include "simpleio.h"

static
const char*
Apic::reg_lvt_bit_str(Apic::Reg reg, Unsigned32 val, int bit)
{
  static const char * const delivery_mode[] =
    { "fixed", "???", "SMI", "???", "NMI", "INIT", "???", "ExtINT" };

  unsigned bits;
  switch (reg)
    {
    case Reg::Lvtt:    bits = Mask | Delivery_state;                    break;
    case Reg::Lvt0:
    case Reg::Lvt1:    bits = Mask | Trigger_mode | Remote_irr | Pin_polarity
                                   | Delivery_state | Delivery_mode;    break;
    case Reg::Lvterr:  bits = Mask | Delivery_state;                    break;
    case Reg::Lvtpc:   bits = Mask | Delivery_state | Trigger_mode;     break;
    case Reg::Lvtthmr: bits = Mask | Delivery_state | Trigger_mode;     break;
    default:           bits = 0; break;
    }

  if ((bits & bit) == 0)
    return "";

  switch (bit)
    {
    case Mask:
      return val & APIC_lvt_masked ? "masked" : "unmasked";
    case Trigger_mode:
      return val & APIC_lvt_level_trigger ? "level" : "edge";
    case Remote_irr:
      return val & APIC_lvt_remote_irr ? "IRR" : "";
    case Pin_polarity:
      return val & APIC_input_polarity ? "active low" : "active high";
    case Delivery_state:
      return val & APIC_snd_pending ? "pending" : "idle";
    case Delivery_mode:
      return delivery_mode[reg_delivery_mode(val)];
    }

  return "";
}

PUBLIC static
void
Apic::reg_show(Apic::Reg reg)
{
  Unsigned32 val = reg_read(reg);

  printf("%-9s%-6s%-4s%-8s%-7s%02x",
         reg_lvt_bit_str(reg, val, Mask),
         reg_lvt_bit_str(reg, val, Trigger_mode),
         reg_lvt_bit_str(reg, val, Remote_irr),
         reg_lvt_bit_str(reg, val, Delivery_state),
         reg_lvt_bit_str(reg, val, Delivery_mode),
         reg_lvt_vector(val));
}

PUBLIC static
void
Apic::regs_show(int indent = 0)
{
  printf("%*sVectors:   LINT0: ", indent, ""); reg_show(Reg::Lvt0);
  printf("\n%*sLINT1: ", indent + 11, ""); reg_show(Reg::Lvt1);
  printf("\n%*sTimer: ", indent + 11, ""); reg_show(Reg::Lvtt);
  printf("\n%*sError: ", indent + 11, ""); reg_show(Reg::Lvterr);
  if (have_pcint())
    {
      printf("\n%*sPerfCnt: ", indent + 9, "");
      reg_show(Reg::Lvtpc);
    }
  if (have_tsint())
    {
      printf("\n%*sThermal: ", indent + 9, "");
      reg_show(Reg::Lvtthmr);
    }
  putchar('\n');
}

PUBLIC static
void
Apic::timer_show(int indent = 0)
{
  printf("%*sTimer mode: %s  counter: %08x/%08x\n",
         indent, "",
	 reg_read(Reg::Lvtt) & APIC_lvt_timer_periodic
	   ? "periodic" : "one-shot",
	 timer_reg_read_initial(), timer_reg_read());
}

PUBLIC static
void
Apic::id_show(int indent = 0)
{
  printf("%*sAPIC id: %02x version: %02x\n",
         indent, "", cxx::int_value<Apic_id>(get_id()) >> 24, get_version());
}

static
void
Apic::bitfield_show(Apic::Reg reg, const char *name, char flag, int indent)
{
  printf("%*s%-11s    0123456789abcdef0123456789abcdef"
                     "0123456789abcdef0123456789abcdef\n", indent, "", name);
  for (unsigned i = 0; i < 8; i++)
    {
      if (!(i & 1))
	printf("%*s%02x ", indent + 12, "", i * 0x20);
      Unsigned32 val = reg_read(Reg{static_cast<unsigned>(reg) + i * 0x10});
      for (unsigned j = 0; j < 32; j++)
	putchar(val & (1<<j) ? flag : '.');
      if (i & 1)
	putchar('\n');
    }
}

PUBLIC static
void
Apic::irr_show(int indent = 0)
{
  Apic::bitfield_show(Reg::Irr, "Ints Reqst:", 'R', indent);
}

PUBLIC static
void
Apic::isr_show(int indent = 0)
{
  Apic::bitfield_show(Reg::Isr, "Ints InSrv:", 'S', indent);
}

