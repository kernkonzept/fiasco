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

PRIVATE
void
Alternative_insn::enable() const
{
  if constexpr (Debug)
    {
      printf("  replace insn at %p/%d with ", disabled_insn(), len);
      if (enabled == 0)
        printf("NOPs\n");
      else
        printf("%p\n", enabled_insn());
    }

  auto *insn = static_cast<Unsigned8 *>(disabled_insn());
  if (enabled == 0)
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
}

IMPLEMENT inline NEEDS["boot_info.h"]
void
Alternative_insn::patch_finish()
{
  Boot_info::reset_checksum_ro();
}
