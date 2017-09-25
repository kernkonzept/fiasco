INTERFACE:

#include <types.h>

/**
 * Single feature dependent instruction alternative.
 *
 * Alternatives are patched at boot time to replace feature dependent
 * instructions.
 *
 * The curren timplementation is limited to patching a single ASM (32bit)
 * instruction.
 *
 * An array of Alternative_insn structures is provided between
 * _alt_insns_begin and _alt_insns_end.
 */
struct Alternative_insn
{
  Signed32 orig; ///< offset of the original instruction relative to `this`
  Signed32 alt;  ///< offset of the alternative instruction relative to `this`
  Unsigned32 feature; ///< feature bit that enables the alternative
  Unsigned8 total_len; ///< Total number of bytes in the code
  Unsigned8 r_len;     ///< Length of this replacement in bytes
};

#define ASM_ALTERNATIVE_ENTRY(feature, idx) \
        "888:                      \n\t"    \
        ".long 819b - 888b         \n\t"    \
        ".long 889" # idx "f - 888b\n\t"    \
        ".word " # feature "       \n\t"    \
        ".byte 839b - 819b         \n\t"    \
        ".byte 8991f - 8891f       \n\t"

#define ALTERNATIVE_INSN(orig_insn, new_insn, feature)    \
        "819:\n\t"                                        \
        orig_insn "\n\t"                                  \
        "829:\n\t"                                        \
        ".skip -(((8991f - 8891f) - (829b - 819b)) > 0) * "   \
        "        ((8991f - 8891f) - (829b - 819b)), 0x00\n\t" \
        "839:\n\t"                                        \
        ".pushsection .alt_insns, \"a?\"\n\t"             \
        ASM_ALTERNATIVE_ENTRY(feature, 1)                 \
        ".popsection \n\t"                                \
        ".pushsection .alt_insn_replacement, \"ax\" \n\t" \
        "8891:\n\t"                                       \
        new_insn "\n\t"                                   \
        "8991:\n\t"                                       \
        ".popsection                   \n\t"

IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include "asm_mips.h"

PUBLIC inline
Unsigned32 *
Alternative_insn::orig_code() const
{
  return reinterpret_cast<Unsigned32*>(reinterpret_cast<Address>(this) + orig);
}

PUBLIC inline
Unsigned32 const *
Alternative_insn::alt_insn() const
{
  return reinterpret_cast<Unsigned32*>(reinterpret_cast<Address>(this) + alt);
}


PRIVATE inline NOEXPORT
void
Alternative_insn::replace() const
{
  Unsigned32 *orig_insn = orig_code();
  Unsigned32 const *alt_insn = this->alt_insn();
  memcpy(orig_insn, alt_insn, r_len);
  if (r_len < total_len)
    memset((char *)orig_insn + r_len, 0, total_len - r_len);

  // sync insn cache, this code does not use synci_step but uses 4byte steps
  for (unsigned i = 0; i <= total_len / 4; ++i)
    asm volatile ("synci %0" : : "R"(orig_insn[i]));

  Mword dummy;
  asm volatile ("sync; " ASM_LA " %0, 1f; jr.hb %0; nop; 1: nop;" : "=r"(dummy));
}

PUBLIC static
void
Alternative_insn::handle_alternatives(unsigned features)
{
  extern Alternative_insn const _alt_insns_begin[];
  extern Alternative_insn const _alt_insns_end[];

  if (0)
    printf("patching alternative instructions for feature: %x\n", features);

  for (auto *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if (i->feature & features)
        {
          if (0)
            printf("  replace insn at %p %08x -> %08x\n",
                   i->orig_code(), *i->orig_code(), *i->alt_insn());
          i->replace();
        }
    }

  // finally clear all instruction hazards
  asm volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      ".set noat\n\t"
      "sync\n\t"
      ASM_LA " $at, 1f\n\t"
      "jr.hb $at\n\t"
      "  nop\n\t"
      "1:\n\t"
      ".set pop"
      );
}

