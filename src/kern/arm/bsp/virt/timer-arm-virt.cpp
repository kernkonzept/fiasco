// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer]:

EXTENSION class Timer
{
public:
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 29;
      case Generic_timer::Virtual:  return 27;
      case Generic_timer::Hyp:      return 26;
      };
  }
};

// --------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_generic_timer]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
