INTERFACE:

EXTENSION struct Alternative_static_functor
{
public:
  inline ALWAYS_INLINE operator bool()
  {
    asm inline goto (ALTERNATIVE_INSN("b %l[no]", "nop")
                     : : [alt_probe] "i"(BASE::probe) : : no);
    return true;
  no:
    return false;
  }
};

IMPLEMENTATION:

#include <cstring>
#include "mem.h"

PRIVATE
void
Alternative_insn::enable() const
{
  auto *insn = static_cast<void *>(disabled_insn());
  auto *enabled_insn = static_cast<void const *>(this->enabled_insn());
  memcpy(insn, enabled_insn, len);
}

IMPLEMENT inline NEEDS["mem.h"]
void
Alternative_insn::patch_finish()
{
  // Mem::dsb() already included in Mem_unit::make_coherent_to_pou()
  Mem::isb();
}
