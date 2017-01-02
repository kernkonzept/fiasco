IMPLEMENTATION [arm_generic_timer && pf_sunxi]:

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 29;
    case Generic_timer::Virtual:  return 27;
    case Generic_timer::Hyp:      return 26;
    };
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
}
