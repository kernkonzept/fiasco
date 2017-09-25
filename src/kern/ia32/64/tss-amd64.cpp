
INTERFACE[amd64]:

#include "l4_types.h"

class Tss
{
public:
  Unsigned32  _ign0; // ignored
  Address     _rsp0;
  Address     _rsp1;
  Address     _rsp2;
  Unsigned32  _ign1; // ignored
  Unsigned32  _ign2; // ignored
  Address     _ist1;
  Address     _ist2;
  Address     _ist3;
  Address     _ist4;
  Address     _ist5;
  Address     _ist6;
  Address     _ist7;
  Unsigned32  _ign3; // ignored
  Unsigned32  _ign4; // ignored
  Unsigned16  _ign5; // ignored
  Unsigned16  _io_bit_map_offset;
} __attribute__((packed));

IMPLEMENTATION:

PUBLIC inline
void
Tss::set_ss0(unsigned)
{}

//-
