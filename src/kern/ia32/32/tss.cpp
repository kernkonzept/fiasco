
INTERFACE:

#include "l4_types.h"

class Tss
{
public:
  Unsigned32  _back_link;
  Address     _esp0;
  Unsigned32  _ss0;
  Address     _esp1;
  Unsigned32  _ss1;
  Address     _esp2;
  Unsigned32  _ss2;
  Unsigned32  _cr3;
  Unsigned32  _eip;
  Unsigned32  _eflags;
  Unsigned32  _eax;
  Unsigned32  _ecx;
  Unsigned32  _edx;
  Unsigned32  _ebx;
  Unsigned32  _esp;
  Unsigned32  _ebp;
  Unsigned32  _esi;
  Unsigned32  _edi;
  Unsigned32  _es;
  Unsigned32  _cs;
  Unsigned32  _ss;
  Unsigned32  _ds;
  Unsigned32  _fs;
  Unsigned32  _gs;
  Unsigned32  _ldt;
  Unsigned16  _trace_trap;
  Unsigned16  _io_bit_map_offset;
} __attribute__((packed));

IMPLEMENTATION:

PUBLIC inline
void
Tss::set_ss0(unsigned ss)
{ _ss0 = ss; }

