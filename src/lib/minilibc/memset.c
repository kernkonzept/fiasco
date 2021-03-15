#include <stddef.h>
#include <string.h>

/* Fiasco is built with strict aliasing which is violated by the code below */
typedef __UINT8_TYPE__  __attribute__((may_alias)) U8;
typedef __UINT16_TYPE__ __attribute__((may_alias)) U16;
typedef __UINT32_TYPE__ __attribute__((may_alias)) U32;
typedef __UINT64_TYPE__ __attribute__((may_alias)) U64;

void * memset(void * dst, int s, size_t count)
{
  if (!count)
    return dst;

  U8 s8 = (U8)s;
  U8 *d8 = dst;
  while (count && ((__UINTPTR_TYPE__)d8 & 0x7U))
    {
      *d8++ = s8;
      count--;
    }

  if (!count)
    return dst;

  /* Replicate value into each byte of the 32-bit word. Gcc *does* understand
   * this statement and emits shift+or instructions or keeps the
   * multiplication, depending on the chosen -march/-mcpu/-mtune setting. */
  U32 s32 = 0x01010101UL * s8;
  U64 s64 = (U64)s32 << 32 | s32;

  U64 *d64 = (U64 *)d8;
  for (; count >= 16; d64 += 2, count -= 16U)
    {
      d64[0] = s64;
      d64[1] = s64;
    }

  if (count & 0x8U)
    *d64++ = s64;

  U32 *d32 = (U32 *)d64;
  if (count & 0x4U)
    *d32++ = s32;

  U16 *d16 = (U16 *)d32;
  if (count & 0x2U)
    *d16++ = (U16)s32;

  d8 = (U8 *)d16;
  if (count & 0x1U)
    *d8 = s8;

  return dst;
}
