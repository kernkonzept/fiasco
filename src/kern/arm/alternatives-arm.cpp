INTERFACE:

#include "std_macros.h"

EXTENSION struct Alternative_static_functor
{
public:
  inline ALWAYS_INLINE operator bool()
  {
    asm inline goto (ALTERNATIVE_INSN(INST32("b")" %l[no]",
                                      INST32("nop"))
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
