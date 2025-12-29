IMPLEMENTATION [arm && dt]:

EXTENSION class Dt
{
public:
  static unsigned
  get_arm_gic_irq_array(Dt::Array_prop<3> ints, unsigned idx)
  {
    if (idx >= ints.elements())
      return ~0u;

    unsigned type = ints.get(idx, 0);
    unsigned val  = ints.get(idx, 1);

    // Translate to global number space
    switch (type)
      {
      case 0: return val + 32; // SPI
      case 1: return val + 16; // PPI
      // case 2: Extended SPI
      // case 3: Extended PPI
      default: return ~0u;
      };
  }

  static unsigned
  get_arm_gic_irq(Dt::Node node, unsigned idx)
  {
    Dt::Array_prop<3> ints = node.get_prop_array("interrupts", { 1, 1, 1 });
    return get_arm_gic_irq_array(ints, idx);
  }

  static unsigned
  get_arm_gic_irq(Dt::Node node, const char *irq_name)
  {
    unsigned irq = ~0u;
    node.stringlist_for_each("interrupt-names",
                             [&](unsigned idx, const char *string)
                             {
                               if (!strcmp(string, irq_name))
                                 {
                                   irq = get_arm_gic_irq(node, idx);
                                   return Dt::Break;
                                 }
                               return Dt::Continue;
                             });
    return irq;
  }
};
