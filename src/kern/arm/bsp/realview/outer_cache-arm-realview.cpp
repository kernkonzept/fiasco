// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview && outer_cache_l2cxx0 && armca9]:

IMPLEMENT
Mword
Outer_cache::platform_init(Mword aux_control)
{
  l2cxx0->write<Mword>(0, L2cxx0::TAG_RAM_CONTROL);
  l2cxx0->write<Mword>(0, L2cxx0::DATA_RAM_CONTROL);
  aux_control &= 0xc0000fff;
  aux_control |= 1 << 17; // 16kb way size
  aux_control |= 1 << 20; // event monitor bus enable
  aux_control |= 1 << 22; // shared attribute ovr enable
  aux_control |= 1 << 28; // data prefetch
  aux_control |= 1 << 29; // insn prefetch
  return aux_control;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview && outer_cache_l2cxx0 && mpcore]:

IMPLEMENT
Mword
Outer_cache::platform_init(Mword aux_control)
{
  aux_control &= 0xfe000fff; // keep latencies, keep reserved, keep NS bits
  aux_control |= 8 << 13; // 8-way associative
  aux_control |= 4 << 17; // 128kb Way size
  aux_control |= 1 << 22; // shared bit ignore
  return aux_control;
}
