// ------------------------------------------------------------------------
IMPLEMENTATION [arm && imx6ul && arm_generic_timer]:

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical:
    case Generic_timer::Virtual: return 27;
    case Generic_timer::Hyp:     return 26;
    };
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
