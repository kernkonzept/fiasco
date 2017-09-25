IMPLEMENTATION [arm && pic_gic && pf_layerscape]:

IMPLEMENT_OVERRIDE inline
Unsigned32 Gic::pcpu_to_sgi(Cpu_phys_id cpu)
{
  // mpidr is set to (aff2, aff1, aff0) = (0xf, 0, <cpu number>), we
  // extract the lower 4 bits as sgi target
  return cxx::int_value<Cpu_phys_id>(cpu) & 0xf;
}
