#include <stddef.h>

/*
 * Define prototype here because ia32 architectures define inline version in
 * <string.h>.
 */
void* memcpy(void* dst, const void* src, size_t count);

/* Fiasco is built with strict aliasing which is violated by the code below */
typedef __UINT8_TYPE__  __attribute__((may_alias)) U8;
typedef __UINT16_TYPE__ __attribute__((may_alias)) U16;
typedef __UINT32_TYPE__ __attribute__((may_alias)) U32;
typedef __UINT64_TYPE__ __attribute__((may_alias)) U64;

#define unlikely(x)    __builtin_expect(!!(x), 0)

void* memcpy(void* dst, const void* src, size_t count)
{
  /*
   * Do not bother with unaligned case. It should not be present in critical
   * paths of Fiasco.
   */
  if (unlikely(((__UINTPTR_TYPE__)dst & 0x03U) ||
               ((__UINTPTR_TYPE__)src & 0x03U)))
    {
      char *d = dst;
      const char *s = src;
      while (count--)
        *d++ = *s++;
      return dst;
    }

  U32 *d32 = dst;
  U32 const *s32 = src;

  /* Copy 2x64bit if possible. Both sides must be congruent! */
  if (count >= 20 && ((__UINTPTR_TYPE__)d32 & 0x7U) ==
                     ((__UINTPTR_TYPE__)s32 & 0x7U))
    {
      if ((__UINTPTR_TYPE__)d32 & 0x7U)
        {
          *d32++ = *s32++;
          count -= 4;
        }
      for (; count >= 16; s32 += 4, d32 += 4, count -= 16U)
        {
          ((U64 *)d32)[0] = ((U64 *)s32)[0];
          ((U64 *)d32)[1] = ((U64 *)s32)[1];
        }
    }
  else
    {
      for (; count >= 16; s32 += 4, d32 += 4, count -= 16U)
        {
          d32[0] = s32[0];
          d32[1] = s32[1];
          d32[2] = s32[2];
          d32[3] = s32[3];
        }
    }

  for (; count >= 4; count -= 4U)
    *d32++ = *s32++;

  U16 *d16 = (U16 *)d32;
  U16 const *s16 = (U16 const *)s32;
  if (count & 0x2U)
    *d16++ = *s16++;

  U8 *d8 = (U8 *)d16;
  U8 const *s8 = (U8 const *)s16;
  if (count & 0x1U)
    *d8 = *s8;

  return dst;
}
