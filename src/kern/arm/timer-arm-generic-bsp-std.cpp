IMPLEMENTATION [arm_generic_timer]:

PUBLIC static inline
unsigned Timer::irq()
{
  using T = Generic_timer::Timer_type;
  static_assert(   static_cast<T>(Gtimer::Type) == Generic_timer::Physical
                || static_cast<T>(Gtimer::Type) == Generic_timer::Virtual
                || static_cast<T>(Gtimer::Type) == Generic_timer::Hyp);

  switch (Gtimer::Type)
    {
    case Generic_timer::Physical:   return 29;
    case Generic_timer::Virtual:    return 27;
    case Generic_timer::Hyp:        return 26;
    case Generic_timer::Secure_hyp: return 20;
    };
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
