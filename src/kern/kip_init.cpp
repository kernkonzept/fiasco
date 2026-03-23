INTERFACE:

#include <cstring>
#include "initcalls.h"
#include "kip.h"

class Kip_init
{
public:
  static void init() FIASCO_INIT;
  static void init_kip_clock() FIASCO_INIT;

private:
  union Kip_initializer
  {
    Kip       k;
    Unsigned8 b[Config::PAGE_SIZE];

    /** Copy a function to the KIP. */
    void copy_fn(unsigned offset, char fn_beg[], char fn_end[])
    { memcpy(b + offset, fn_beg, mem_range_bytes(fn_beg, fn_end)); }

    /** Set machine word at KIP with a given offset. */
    void set_mword(unsigned offset, Mword value)
    { *reinterpret_cast<Mword *>(b + offset) = value; }

    /** Set 64-bit value at KIP with a given offset. */
    void set_uint64(unsigned offset, Unsigned64 value)
    { *reinterpret_cast<Unsigned64 *>(b + offset) = value; }

    /** Make caches coherent after copying executable code to the KIP. */
    void make_coherent() const;
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64 || arm || riscv || mips]:

#include "kip_offsets.h"
#include "mem_unit.h"

IMPLEMENT inline NEEDS["kip_offsets.h", "mem_unit.h"]
void
Kip_init::Kip_initializer::make_coherent() const
{
  size_t sz = OFFS__KIP_FN_CODE_END - OFFS__KIP_FN_CODE_START;
  Mem_unit::make_coherent_to_pou(b + OFFS__KIP_FN_CODE_START, sz);
}
