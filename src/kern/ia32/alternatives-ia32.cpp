INTERFACE:

EXTENSION struct Alternative_static_functor
{
public:
  inline ALWAYS_INLINE operator bool()
  {
    asm inline goto (ALTERNATIVE_INSN_ENABLED_NOP(
                    ".byte 0xe9; .4byte %l[no] - 1f; 1:")
                    : : [alt_probe] "i"(BASE::probe) : : no);
    return true;
  no:
    return false;
  }
};

IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include "boot_info.h"
#include "mem_unit.h"

PRIVATE inline NOEXPORT
void
Alternative_insn::enable() const
{
  auto *insn = static_cast<Unsigned8 *>(disabled_insn());
  if (this->enabled == 0)
    {
      // Replace "disabled instructions" by NOPs. Taken from Intel SDM.
      // 32-bit variants are equivalent to 64-bit variants except s/rax/eax/
      for (Unsigned8 *insn_end = insn + len; insn < insn_end;)
        {
          int l = insn_end - insn;
          if (l >= 8)
            {
              // nop dword ptr [rax + rax*1 + 0]
              insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x84; insn[3] = 0x00;
              insn[4] = 0x00; insn[5] = 0x00; insn[6] = 0x00; insn[7] = 0x00;
              insn += 8;
            }
          else if (l >= 7)
            {
              // nop dword ptr [rax + 0]
              insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x80; insn[3] = 0x00;
              insn[4] = 0x00; insn[5] = 0x00; insn[6] = 0x00;
              insn += 7;
            }
          else if (l >= 6)
            {
              // nop word ptr [rax + rax*1 + 0]
              insn[0] = 0x66; insn[1] = 0x0f; insn[2] = 0x1f; insn[3] = 0x44;
              insn[4] = 0x00; insn[5] = 0x00;
              insn += 6;
            }
          else if (l >= 5)
            {
              // nop dword ptr [rax + rax*1 + 0]
              insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x44; insn[3] = 0x00;
              insn[4] = 0x00;
              insn += 5;
            }
          else if (l >= 4)
            {
              // nop dword ptr [rax + 0]
              insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x40; insn[3] = 0x00;
              insn += 4;
            }
          else if (l >= 3)
            {
              // nop dword ptr [rax]
              insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x00;
              insn += 3;
            }
          else if (l >= 2)
            {
              // xchg ax,ax
              insn[0] = 0x66; insn[1] = 0x90;
              insn += 2;
            }
          else
            {
              // nop
              insn[0] = 0x90;
              insn += 1;
            }
        }
    }
  else
    {
      // Replace "disabled instructions" by "enabled instructions".
      auto *enabled_insn = static_cast<Unsigned8 const *>(this->enabled_insn());
      memcpy(insn, enabled_insn, len);
    }
  Mem_unit::make_coherent_to_pou(insn, len);
}

IMPLEMENT
void
Alternative_insn::init()
{
  extern Alternative_insn const _alt_insns_begin[];
  extern Alternative_insn const _alt_insns_end[];

  if constexpr (Debug)
    printf("patching alternative instructions\n");

  if (&_alt_insns_begin[0] == &_alt_insns_end[0])
    return;

  for (auto const *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if (i->probe())
        {
          if constexpr (Debug)
            {
              printf("  replace insn at %p/%d with ",
                     static_cast<void *>(i->disabled_insn()), i->len);
              if (i->enabled == 0)
                printf("NOPs\n");
              else
                printf("%p\n", i->enabled_insn());
            }
          i->enable();
        }
    }

  Boot_info::reset_checksum_ro();
}
