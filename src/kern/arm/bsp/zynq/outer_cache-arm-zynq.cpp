IMPLEMENTATION [arm && pf_zynq && arm_cache_l2cxx0]:

IMPLEMENT
Mword
Outer_cache::platform_init(Mword aux_control)
{
  // 64k way, 8-way associativityciativity
  return (aux_control & 0xf2f0ffff) | 0x00060000;
}
