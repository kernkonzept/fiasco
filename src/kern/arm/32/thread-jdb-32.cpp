IMPLEMENTATION [arm && 32bit && debug]:

PRIVATE static inline int
Thread::arm_enter_debugger(Trap_state *ts, Cpu_number log_cpu,
                           unsigned long *ntr, void *stack)
{
  Mword dummy1, tmp;
  register Mword _ts asm("r0") = (Mword)ts;
  register Cpu_number _lcpu asm("r1") = log_cpu;

  asm volatile(
      "mov    %[origstack], sp         \n"
      "ldr    %[tmp], [%[ntr]]         \n"
      "teq    %[tmp], #0               \n"
      "moveq  sp, %[stack]             \n"
      "add    %[tmp], %[tmp], #1       \n"
      "str    %[tmp], [%[ntr]]         \n"
      "str    %[origstack], [sp, #-4]! \n"
      "str    %[ntr], [sp, #-4]!       \n"
      "adr    lr, 1f                   \n"
      "mov    pc, %[handler]           \n"
      "1:                              \n"
      "ldr    %[ntr], [sp], #4         \n"
      "ldr    sp, [sp]                 \n"
      "ldr    %[tmp], [%[ntr]]         \n"
      "sub    %[tmp], %[tmp], #1       \n"
      "str    %[tmp], [%[ntr]]         \n"
      : [origstack] "=&r" (dummy1), [tmp] "=&r" (tmp),
        "=r" (_ts), "=r" (_lcpu)
      : [ntr] "r" (ntr), [stack] "r" (stack),
        [handler] "r" (*nested_trap_handler),
        "2" (_ts), "3" (_lcpu)
      : "memory", "r2", "r3", "r9", "r12", "r14");

  return _ts;
}

