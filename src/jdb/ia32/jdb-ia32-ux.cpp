INTERFACE[ia32,amd64,ux]:

#include "jdb_core.h"
#include "jdb_handler_queue.h"
#include "jdb_entry_frame.h"
#include "trap_state.h"

class Space;
class Thread;

// ------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64 || ux]:

#include "config.h"
#include "div32.h"
#include "kernel_console.h"
#include "paging.h"
#include "jdb_screen.h"

PUBLIC static
void
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 uns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);
  Unsigned32 ums = div32(uns, 1000000);

  if (tsc < 0)
    uns = -uns;

  if (ums >= 3600000000U)
    {
      buf->printf(">999 h ");
      return;
    }

  if (sign)
    buf->printf("%c", (tsc < 0) ? '-' : (tsc == 0) ? ' ' : '+');

  if (ums >= 60000000)
    {
      // 1:00...999:00 h
      Mword _h  = ums / 3600000;
      Mword _m  = (ums - 3600000 * _h) / 60000;
      buf->printf("%3lu:%02lu     h ", _h, _m);
      return;
    }

  if (ums >= 1000000)
    {
      // 1:00...999:00 min
      Mword _m  = ums / 60000;
      Mword _s  = (ums - 60000 * _m) / 1000;
      buf->printf("%3lu:%02lu    min", _m, _s);
      return;
    }

  if (ums >= 1000)
    {
      // 1.000000...999.000000 s
      Mword _s  = ums / 1000;
      Mword _us = div32(uns, 1000) - 1000000 * _s;
      buf->printf("%3lu.%06lu s ", _s, _us);
      return;
    }

  if (uns == 0)
    {
      buf->printf("  0          ");
      return;
    }

  // 1.000000...999.000000 ms
  Mword _ms = ums;
  Mword _ns = ((Mword)uns - 1000000 * _ms);
  buf->printf("%3lu.%06lu ms", _ms, _ns);
}

PUBLIC static
void
Jdb::write_tsc(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 ns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);
  if (tsc < 0)
    ns = -ns;
  write_ll_ns(buf, ns, sign);
}

PUBLIC
static int
Jdb::get_register(char *reg)
{
  union
  {
    char c[4];
    Unsigned32 v;
  } reg_name;
  int i;
  reg_name.v = 0;

  putchar(reg_name.c[0] = Jdb_screen::Reg_prefix);

  for (i = 1; i < 3; i++)
    {
      int c = getchar();
      if (c == KEY_ESC)
	return false;
      putchar(reg_name.c[i] = c & 0xdf);
      if (c == '8' || c == '9')
	break;
    }

  reg_name.c[3] = '\0';

  for (i = 0; i < Jdb_screen::num_regs(); i++)
    if (reg_name.v == *((Unsigned32 *)(Jdb_screen::Reg_names[i])))
      break;

  if (i == Jdb_screen::num_regs())
    return false;

  *reg = i + 1;
  return true;
}
