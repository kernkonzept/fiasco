INTERFACE:

#include <types.h>

/**
 * Single feature dependent instruction alternative.
 *
 * Alternatives are patched at boot time dependent on the return value of the
 * probe function. The array of Alternative_insn structures is provided between
 * _alt_insns_begin and _alt_insns_end by the linker script.
 *
 * On certain platforms, `enabled=0` means to use NOPs corresponding to `len`
 * instead of explicit "enabled" instructions.
 */
class Alternative_insn
{
public:
  enum { Debug = 0 };

  /**
   * Initialize the alternative instructions.
   *
   * Go through all blocks and replace "disabled instructions" by "enabled
   * instructions" if the corresponding probe() function returns `true`.
   */
  static void init();

private:
  static void patch_finish();

  inline void *disabled_insn() const
  {
    return offset_cast<void *>(this, disabled);
  }

  inline void const *enabled_insn() const
  {
    return offset_cast<void *>(this, enabled);
  }

  /** function called to determine which version is used. */
  bool (*probe)();
  /** offset of the "disabled" instruction relative to `this` */
  Signed32 disabled;
  /** offset of the "enabled" instruction relative to `this` */
  Signed32 enabled;
  /** Number of bytes in the code */
  Unsigned8 len;

} __attribute__((packed));

#if defined(__aarch64__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".dword %c[alt_probe]"
#elif defined(__arm__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".word %c[alt_probe]"
#elif defined(__x86_64__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".quad %c[alt_probe]"
#elif defined(__i386__) || defined(__i686__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".long %c[alt_probe]"
#else
# error "Implementation requires a GNU compiler!"
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
 * asm volatile (ALTERNATIVE_INSN("mov $0, %0", "mov $1, %0")
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

//----------------------------------------------------------------------------
INTERFACE [ia32 || amd64]:

/**
 * Define alternative inline assembly versions.
 *
 * This macro is similar to ALTERNATIVE_INSN but only expects the instructions
 * which are disabled, that is, overridden by NOPs in case alt_probe() returns
 * true:
 *
 * \code
 * bool my_probe()
 * {
 *   return ...;
 * }
 *
 * asm volatile (ALTERNATIVE_INSN("mov $0, %0", "mov $1, %0")
 *               : "=r"(variable)
 *               : [alt_probe] "i"(my_probe) );
 * \endcode
 *
 * \param disabled_insn String of assembler instruction that are used if
 *                      alt_probe() returns false.
 */
#define ALTERNATIVE_INSN_ENABLED_NOP(disabled_insn) \
        "819:                                           \n\t" \
        disabled_insn                                  "\n\t" \
        "829:                                           \n\t" \
        ".pushsection .alt_insns, \"a?\"                \n\t" \
        "888:                                           \n\t" \
        ASM_ALTERNATIVE_ENTRY_PTR                      "\n\t" \
        ".4byte 819b - 888b                             \n\t" \
        ".4byte 0                                       \n\t" \
        ".byte 829b - 819b                              \n\t" \
        ".popsection                                    \n\t"

//----------------------------------------------------------------------------
INTERFACE:

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
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include "mem_unit.h"

PRIVATE inline NEEDS["mem_unit.h"]
void
Alternative_insn::make_coherent() const
{
  Mem_unit::make_coherent_to_pou(disabled_insn(), len);
}

IMPLEMENT
void
Alternative_insn::init()
{
  extern Alternative_insn const _alt_insns_begin[];
  extern Alternative_insn const _alt_insns_end[];

  auto const *begin = &_alt_insns_begin[0];
  auto const *end = &_alt_insns_end[0];

  if constexpr (Debug)
    printf("Patching alternative instructions.\n");

  if (begin != end)
    {
      for (auto *i = begin; i != end; ++i)
        if (i->probe())
          {
            i->enable();
            i->make_coherent();
          }

      Alternative_insn::patch_finish();
    }

  if constexpr (Debug)
    printf("Patching done.\n");
}
