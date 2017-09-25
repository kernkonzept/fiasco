IMPLEMENTATION [ia32,amd64]:

PUBLIC static inline void
Platform_control::prepare_cpu_suspend(Cpu_number)
{
  asm volatile ("wbinvd");
}

PUBLIC static inline void FIASCO_NORETURN
Platform_control::cpu_suspend(Cpu_number)
{
  for (;;)
    asm volatile ("cli; wbinvd; hlt");
}
