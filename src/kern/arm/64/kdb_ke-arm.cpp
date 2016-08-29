IMPLEMENTATION [arm]:

inline void kdb_ke(const char *msg)
{
  unsigned long x0 asm("x0") = (unsigned long)msg;
  asm volatile ("brk #0" : : "r"(x0));
}

inline void kdb_ke_sequence(const char *msg)
{
  unsigned long x0 asm("x0") = (unsigned long)msg;
  asm volatile ("brk #1" : : "r"(x0));
}

