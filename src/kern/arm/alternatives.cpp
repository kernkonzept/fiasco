INTERFACE:

#include <types.h>

/**
 * Single feature dependent instruction alternative.
 *
 * Alternatives are patched at boot time dependent on the return value of the
 * probe function. The array of Alternative_insn structures is provided between
 * _alt_insns_begin and _alt_insns_end by the linker script.
 */
struct Alternative_insn
{
  bool (*probe)(); ///< function called to determine which version is used
  Signed32 disabled; ///< offset of the "disabled" instruction relative to `this`
  Signed32 enabled; ///< offset of the "enabled" instruction relative to `this`
  Unsigned8 len; ///< Number of bytes in the code
} __attribute__((packed));

#if defined(__aarch64__)
#define ASM_ALTERNATIVE_ENTRY_PTR ".dword %c[alt_probe]"
#elif defined(__arm__)
#define ASM_ALTERNATIVE_ENTRY_PTR ".word %c[alt_probe]"
#else
#error "Implementation requires an ARM compiler!"
#endif

/**
 * Define alternative inline assembly versions.
 *
 * The macro is intended to be used in an extended inline assembly statement.
 * During boot time, a probe function is called to select the right instruction
 * alternative. The emitted assembly requires the probe function to be passed
 * as "alt_probe" symbolic name immediate input operand:
 *
 * \code
 * bool my_probe()
 * {
 *   return ...;
 * }
 *
 * asm volatile (ALTERNATIVE_INSN("mov %0, #0", "mov %0, #1")
 *               : "=r"(variable)
 *               : [alt_probe] "i"(my_probe) );
 * \endcode
 *
 * Both versions of the assembly must have the same length. Otherwise one of
 * the ".org" directives will fail. We can't use ".if" because that's not
 * supported by clang.
 *
 * \warning The "enabled" version of the assembler instructions is placed in
 *          a different section and copied over the "disabled" version if
 *          the probe function returns true. Thus you cannot reference other
 *          symbols from the "enabled" version because the linker will place it
 *          at a different address from where it will run!
 *
 * \param disabled_insn String of assembler instruction that are used if
 *                      alt_probe() returns false.
 * \param enabled_insn  String of assembler instruction that are used if
 *                      alt_probe() returns true.
 */
#define ALTERNATIVE_INSN(disabled_insn, enabled_insn)         \
        ".pushsection .alt_insn_replacement, \"ax\"     \n\t" \
        "8891:                                          \n\t" \
        enabled_insn                                   "\n\t" \
        "8991:                                          \n\t" \
        ".popsection                                    \n\t" \
        "819:                                           \n\t" \
        disabled_insn                                  "\n\t" \
        "829:                                           \n\t" \
        ".org . - (8991b - 8891b) + (829b - 819b)       \n\t" \
        ".org . - (829b - 819b) + (8991b - 8891b)       \n\t" \
        "839:                                           \n\t" \
        ".pushsection .alt_insns, \"a?\"                \n\t" \
        "888:                                           \n\t" \
        ASM_ALTERNATIVE_ENTRY_PTR                      "\n\t" \
        ".4byte 819b - 888b                             \n\t" \
        ".4byte 8891b - 888b                            \n\t" \
        ".byte 8991b - 8891b                            \n\t" \
        ".popsection                                    \n\t"

/**
 * Mixin class to create a statically evaluated boolean functor.
 *
 * Such a static key is intended to be used in conditional expressions:
 *
 * \code
 * INTERFACE:
 *
 * #include "alternatives.h"
 *
 * struct some_feature : public Alternative_static_functor<some_feature>
 * {
 *   static bool probe()
 *   {
 *     return true/false;
 *   }
 * };
 *
 * IMPLEMENTATION:
 *
 * void user()
 * {
 *   if (some_feature())
 *    feature_present();
 *  else
 *    feature_absent();
 * }
 * \endcode
 *
 * While it looks like a normal conditional expression the compiler will
 * actually emit static branches. The associated boolean probe() function is
 * called once at boot time. Depending on the return value of the function, the
 * static branch is then patched accordingly.
 */
template<typename BASE>
struct Alternative_static_functor
{
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

PUBLIC inline
Unsigned32 *
Alternative_insn::disabled_insn() const
{
  return reinterpret_cast<Unsigned32*>(reinterpret_cast<Address>(this) + disabled);
}

PUBLIC inline
Unsigned32 const *
Alternative_insn::enabled_insn() const
{
  return reinterpret_cast<Unsigned32*>(reinterpret_cast<Address>(this) + enabled);
}


PRIVATE inline NOEXPORT
void
Alternative_insn::enable() const
{
  Unsigned32 *insn = disabled_insn();
  Unsigned32 const *enabled_insn = this->enabled_insn();
  memcpy(insn, enabled_insn, len);
  Mem_unit::make_coherent_to_pou(insn, len);
}

PUBLIC static
void
Alternative_insn::init()
{
  extern Alternative_insn const _alt_insns_begin[];
  extern Alternative_insn const _alt_insns_end[];

  if (0)
    printf("patching alternative instructions\n");

  if (&_alt_insns_begin[0] == &_alt_insns_end[0])
    return;

  for (auto *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if (i->probe())
        {
          if (0)
            printf("  replace insn at %p/%d\n", i->disabled_insn(), i->len);
          i->enable();
        }
    }

  // Mem::dsb() already included in Mem_unit::make_coherent_to_pou()
  Mem::isb();
}
