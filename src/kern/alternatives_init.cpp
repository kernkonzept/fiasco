INTERFACE:

/**
 * Separate module initializing the alternative instructions.
 *
 * This is necessary to decouple Alternative_insn from mem_unit to avoid a
 * cyclic dependency.
 */
class Alternative_insn_init
{
public:
  /**
   * Initialize the alternative instructions.
   *
   * Go through all blocks and replace "disabled instructions" by "enabled
   * instructions" if the corresponding probe() function returns `true`.
   */
  static void init();
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include "alternatives.h"
#include "mem_unit.h"
#include "static_init.h"

IMPLEMENT static
void
Alternative_insn_init::init()
{
  extern Alternative_insn const _alt_insns_begin[];
  extern Alternative_insn const _alt_insns_end[];

  auto const *begin = &_alt_insns_begin[0];
  auto const *end = &_alt_insns_end[0];

  if (begin != end)
    {
      for (auto *i = begin; i != end; ++i)
        if (i->probe())
          {
            i->enable();
            Mem_unit::make_coherent_to_pou(i->disabled_insn(), i->len);
          }

      patch_finish();
    }
}

STATIC_INITIALIZE_P(Alternative_insn_init, ALT_INSN_INIT_PRIO);

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include "boot_info.h"

PRIVATE static
void
Alternative_insn_init::patch_finish()
{
  Boot_info::reset_checksum_ro();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm]:

PRIVATE static
void
Alternative_insn_init::patch_finish()
{
  // Mem::dsb() already included in Mem_unit::make_coherent_to_pou()
  Mem::isb();
}
