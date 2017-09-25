IMPLEMENTATION [amd64]:

IMPLEMENT static inline
void
Mem::memset_mwords (void *dst, unsigned long value, unsigned long n)
{
  unsigned long dummy1, dummy2;

  asm volatile ("cld					\n\t"
                "rep stosq				\n\t"
                : "=c" (dummy1), "=D" (dummy2)
                : "a"(value), "c" (n), "D" (dst)
                : "memory");
}


IMPLEMENT static inline
void
Mem::memcpy_bytes (void *dst, void const *src, unsigned long n)
{
  unsigned long dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "repz movsq (%%rsi), (%%rdi)		\n\t"
                "mov %%rdx, %%rcx			\n\t"
                "repz movsb (%%rsi), (%%rdi)		\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n >> 3), "d" (n & 7), "S" (src), "D" (dst)
                : "memory");
}

IMPLEMENT static inline
void
Mem::memcpy_mwords (void *dst, void const *src, unsigned long n)
{
  unsigned long dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "rep movsq (%%rsi), (%%rdi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n), "S" (src), "D" (dst)
                : "memory");
}

PUBLIC static inline
void
Mem::memcpy_bytes_fs (void *dst, void const *src, unsigned long n)
{
  unsigned long dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "rep movsq (%%rsi), (%%rdi)	\n\t"
                "mov %%rdx, %%rcx			\n\t"
                "repz movsb (%%rsi), (%%rdi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n >> 3), "d" (n & 7), "S" (src), "D" (dst)
                : "memory");
}

PUBLIC static inline
void
Mem::memcpy_mwords_fs (void *dst, void const *src, unsigned long n)
{
  unsigned long dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "rep movsq (%%rsi), (%%rdi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n), "S" (src), "D" (dst)
                : "memory");
}



// ------------------------------------------------------------------------
IMPLEMENTATION [amd64 && mp]:

IMPLEMENT inline static void Mem::mb()
{ __asm__ __volatile__ ("mfence" : : : "memory"); }

IMPLEMENT inline static void Mem::rmb()
{ __asm__ __volatile__ ("lfence" : : : "memory"); }

IMPLEMENT inline static void Mem::wmb()
{ /* Just barrier should be enough */
  __asm__ __volatile__ ("sfence" : : : "memory");
}

IMPLEMENT inline static void Mem::mp_mb() { mb(); }
IMPLEMENT inline static void Mem::mp_acquire() { mb(); }
IMPLEMENT inline static void Mem::mp_release() { mb(); }
IMPLEMENT inline static void Mem::mp_rmb() { rmb(); }
IMPLEMENT inline static void Mem::mp_wmb() { wmb(); }
