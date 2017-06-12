IMPLEMENTATION [arm && pic_gic && pf_rcar3]:

IMPLEMENT_OVERRIDE inline
Unsigned32 Gic::pcpu_to_sgi(Cpu_phys_id cpu)
{
  unsigned id = cxx::int_value<Cpu_phys_id>(cpu);
  return (id >> 6) | (id & 3);
}
