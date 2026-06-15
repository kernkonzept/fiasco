INTERFACE:

EXTENSION struct Alternative_static_functor
{
public:
  inline ALWAYS_INLINE operator bool()
  {
    asm inline goto (ALTERNATIVE_INSN(
#ifdef __thumb__
                                      "b.w %l[no]", "nop.w"
#else
                                      "b %l[no]", "nop"
#endif
                                      )
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
