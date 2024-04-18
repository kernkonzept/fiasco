IMPLEMENTATION [arm_generic_timer && pf_mps3_an536]:

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical:   return 30;
    case Generic_timer::Virtual:    return 27;
    case Generic_timer::Hyp:        return 26;
    case Generic_timer::Secure_hyp: return 20;
    };
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
}
