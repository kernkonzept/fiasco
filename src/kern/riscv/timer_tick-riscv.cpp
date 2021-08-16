IMPLEMENTATION [riscv]:

IMPLEMENT void Timer_tick::setup(Cpu_number) {}

IMPLEMENT void Timer_tick::enable(Cpu_number) {}

IMPLEMENT void Timer_tick::disable(Cpu_number) {}

IMPLEMENT bool Timer_tick::allocate_irq(Irq_base *, unsigned) { return false; }
