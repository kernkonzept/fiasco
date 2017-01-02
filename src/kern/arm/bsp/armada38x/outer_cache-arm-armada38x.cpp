IMPLEMENTATION [arm && pf_armada38x && arm_cache_l2cxx0]:

IMPLEMENT
Mword
Outer_cache::platform_init(Mword aux_control)
{
  return aux_control;
}
