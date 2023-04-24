IMPLEMENTATION [ia32 || ux]:

IMPLEMENT static inline
void
Mem::memset_mwords (void *dst, unsigned long value, unsigned long n)
{
  unsigned dummy1, dummy2;
  asm volatile ("cld					\n\t"
		"repz stosl               \n\t"
		: "=c"(dummy1), "=D"(dummy2)
		: "a"(value), "c"(n), "D"(dst)
		: "memory");
}


IMPLEMENT static inline
void
Mem::memcpy_bytes (void *dst, void const *src, unsigned long n)
{
  unsigned dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "repz movsl %%ds:(%%esi), %%es:(%%edi)	\n\t"
                "mov %%edx, %%ecx			\n\t"
                "repz movsb %%ds:(%%esi), %%es:(%%edi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n >> 2), "d" (n & 3), "S" (src), "D" (dst)
                : "memory");
}


IMPLEMENT static inline
void
Mem::memcpy_mwords (void *dst, void const *src, unsigned long n)
{
  unsigned dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "rep movsl %%ds:(%%esi), %%es:(%%edi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n), "S" (src), "D" (dst)
                : "memory");
}

PUBLIC static inline
void
Mem::memcpy_bytes_fs (void *dst, void const *src, unsigned long n)
{
  unsigned dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "rep movsl %%fs:(%%esi), %%es:(%%edi)	\n\t"
                "mov %%edx, %%ecx			\n\t"
                "repz movsb %%fs:(%%esi), %%es:(%%edi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n >> 2), "d" (n & 3), "S" (src), "D" (dst)
                : "memory");
}

PUBLIC static inline
void
Mem::memcpy_mwords_fs (void *dst, void const *src, unsigned long n)
{
  unsigned dummy1, dummy2, dummy3;

  asm volatile ("cld					\n\t"
                "rep movsl %%fs:(%%esi), %%es:(%%edi)	\n\t"
                : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                : "c" (n), "S" (src), "D" (dst)
                : "memory");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || ux) && mp]:

IMPLEMENT inline static void Mem::mb()
{ __asm__ __volatile__ ("lock; addl $0,0(%%esp)" : : : "memory"); /* mfence */ }

IMPLEMENT inline static void Mem::rmb() { mb(); /* lfence */ }
IMPLEMENT inline static void Mem::wmb() { barrier(); /* sfence */}

IMPLEMENT inline static void Mem::mp_mb() { mb(); }
IMPLEMENT inline static void Mem::mp_acquire() { mb(); }
IMPLEMENT inline static void Mem::mp_release() { mb(); }
IMPLEMENT inline static void Mem::mp_rmb() { rmb(); }
IMPLEMENT inline static void Mem::mp_wmb() { wmb(); }
