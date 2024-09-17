INTERFACE:

EXTENSION struct Alternative_static_functor
{
public:
  inline ALWAYS_INLINE operator bool()
  {
    asm goto (ALTERNATIVE_INSN("b %l[no]", "nop")
              : : [alt_probe] "i"(BASE::probe) : : no);
    return true;
  no:
    return false;
  }
};

IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include "mem_unit.h"

PRIVATE inline NOEXPORT
void
Alternative_insn::enable() const
{
  void *insn = static_cast<void *>(disabled_insn());
  void const *enabled_insn = static_cast<void const *>(this->enabled_insn());
  memcpy(insn, enabled_insn, len);
  Mem_unit::make_coherent_to_pou(insn, len);
}

IMPLEMENT
void
Alternative_insn::init()
{
  extern Alternative_insn const _alt_insns_begin[];
  extern Alternative_insn const _alt_insns_end[];

  if (Debug)
    printf("patching alternative instructions\n");

  if (&_alt_insns_begin[0] == &_alt_insns_end[0])
    return;

  for (auto *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if (i->probe())
        {
          if (Debug)
            printf("  replace insn at %p/%d\n",
                   static_cast<void *>(i->disabled_insn()), i->len);
          i->enable();
        }
    }

  // Mem::dsb() already included in Mem_unit::make_coherent_to_pou()
  Mem::isb();
}
