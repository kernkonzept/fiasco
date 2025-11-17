IMPLEMENTATION [ia32 || amd64]:

PUBLIC static inline void
Platform_control::prepare_cpu_suspend()
{
  asm volatile ("wbinvd");
}

PUBLIC [[noreturn]] static inline void
Platform_control::cpu_suspend()
{
  for (;;)
    asm volatile ("cli; wbinvd; hlt");
}
