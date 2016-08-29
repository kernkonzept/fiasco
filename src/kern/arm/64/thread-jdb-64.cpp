IMPLEMENTATION [arm && 64bit && debug]:

PRIVATE static inline int
Thread::arm_enter_debugger(Trap_state *ts, Cpu_number log_cpu,
                           unsigned long *ntr, void *stack)
{
  Mword dummy1, tmp;
  register Mword _ts asm("x0") = (Mword)ts;
  register Cpu_number _lcpu asm("x1") = log_cpu;

  asm volatile(
      "mov    %[origstack], sp         \n"
      "ldr    %[tmp], [%[ntr]]         \n"
      "cbnz   %[tmp], 1f               \n"
      "mov    sp, %[stack]             \n"
      "1:                              \n"
      "add    %[tmp], %[tmp], #1       \n"
      "str    %[tmp], [%[ntr]]         \n"
      "str    %[origstack], [sp, #-8]! \n"
      "str    %[ntr], [sp, #-8]!       \n"
      "adr    x30, 1f                  \n"
      "br     %[handler]               \n"
      "1:                              \n"
      "ldr    %[ntr], [sp], #8         \n"
      "ldr    %[origstack], [sp]       \n"
      "mov    sp, %[origstack]         \n"
      "ldr    %[tmp], [%[ntr]]         \n"
      "sub    %[tmp], %[tmp], #1       \n"
      "str    %[tmp], [%[ntr]]         \n"
      : [origstack] "=&r" (dummy1), [tmp] "=&r" (tmp),
      "=r" (_ts), "=r" (_lcpu)
      : [ntr] "r" (ntr), [stack] "r" (stack),
      [handler] "r" (*nested_trap_handler),
      "2" (_ts), "3" (_lcpu)
      : "memory", "x2", "x3", "x4");

  return _ts;
}

