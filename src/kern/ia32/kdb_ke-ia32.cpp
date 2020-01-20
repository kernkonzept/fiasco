IMPLEMENTATION [(ia32,ux,amd64) && debug]:

inline void kdb_ke(char const *msg)
{
  asm volatile ("int3" : : "a"(0), "c"(msg));
}

inline void kdb_ke_nstr(char const *msg, unsigned len)
{
  asm volatile ("int3" : : "a"(1), "c"(msg), "d"(len));
}

inline void kdb_ke_sequence(char const *msg, unsigned len)
{
  asm volatile ("int3" : : "a"(2), "c"(msg), "d"(len));
}
