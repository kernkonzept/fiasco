IMPLEMENTATION [arm && 64bit && debug]:

PRIVATE static inline int
Thread::arm_enter_debugger(Trap_state *ts, Cpu_number log_cpu,
                           unsigned long *ntr, void *stack)
{
  Mword dummy1, tmp;
  register Mword _ts asm("x0") = (Mword)ts;
  register Cpu_number _lcpu asm("x1") = log_cpu;
  register void *handler asm("x4") = (void*)*nested_trap_handler;
  register void *stack_p asm("x3") = stack;

  asm volatile(
      "mov    %[origstack], sp         \n"
      "ldr    %[tmp], [%[ntr]]         \n"
      "cbnz   %[tmp], 1f               \n"
      "mov    sp, %[stack]             \n"
      "1:                              \n"
      "add    %[tmp], %[tmp], #1       \n"
      "str    %[tmp], [%[ntr]]         \n"
      "str    %[ntr], [sp, #-32]!      \n"
      "str    %[origstack], [sp, #8]   \n"
      "str    x29, [sp, #16]           \n"
      "adr    x30, 1f                  \n"
      "br     %[handler]               \n"
      "1:                              \n"
      "ldr    %[ntr], [sp]             \n"
      "ldr    %[origstack], [sp, #8]   \n"
      "ldr    x29, [sp, #16]           \n"
      "mov    sp, %[origstack]         \n"
      "ldr    %[tmp], [%[ntr]]         \n"
      "sub    %[tmp], %[tmp], #1       \n"
      "str    %[tmp], [%[ntr]]         \n"
      :
      [origstack] "=&r" (dummy1), [tmp] "=&r" (tmp),
      "+r" (_ts), "+r" (_lcpu),
      [ntr] "+r" (ntr), [stack] "+r" (stack_p),
      [handler] "+r" (handler)
      :
      :
      "memory", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
      "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30");

  return _ts;
}

