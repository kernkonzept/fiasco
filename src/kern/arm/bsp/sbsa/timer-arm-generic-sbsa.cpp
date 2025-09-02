IMPLEMENTATION [arm_generic_timer && pf_sbsa]:

// ARM Base System Architecture 3.6 - PPI assignments
PUBLIC static inline
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 30;
    case Generic_timer::Virtual:  return 27;
    case Generic_timer::Hyp:      return 26;
    };
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
