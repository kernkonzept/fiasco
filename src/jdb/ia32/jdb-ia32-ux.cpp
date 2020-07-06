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

IMPLEMENT_OVERRIDE
void
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 uns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);

  if (tsc < 0)
    uns = -uns;

  if (sign)
    buf->printf("%c", (tsc < 0) ? '-' : (tsc == 0) ? ' ' : '+');

  Mword _s  = uns / 1000000000;
  Mword _us = div32(uns, 1000) - 1000000 * _s;
  buf->printf("%3lu.%06lu s ", _s, _us);
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
