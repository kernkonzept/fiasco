#pragma once

#include "types.h"
#include "std_macros.h"

class Trap_state;

struct Exc_entry
{
  Mword ip;
  Mword fixup;
  bool FIASCO_FASTCALL (*handler)(Exc_entry const *e, Trap_state *ts);

  struct Exc_vector
  {
    Exc_entry const *b;
    Exc_entry const *e;
    Exc_entry const *begin() const { return b; }
    Exc_entry const *end() const { return e; }
    Exc_vector(Exc_entry const *b, Exc_entry const *e)
    : b(b), e(e) {}
  };

  static Exc_vector table()
  {
    extern Exc_entry const __exc_table_start[];
    extern Exc_entry const __exc_table_end[];
    return Exc_vector(__exc_table_start, __exc_table_end);
  }
};


