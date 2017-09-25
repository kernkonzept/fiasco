#ifndef MLC_MEMCPY_H__
#define MLC_MEMCPY_H__

#include <cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

static inline void *memcpy(void *dest, const void *src, size_t n)
{
  unsigned dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "repz movsb %%ds:(%%esi), %%es:(%%edi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n), "S" (src), "D" (dest)
                : "memory");
  return dest;
}

__END_DECLS

#endif // MLC_MEMCPY_H__

