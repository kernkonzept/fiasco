IMPLEMENTATION [arm && dt]:

EXTENSION class Dt
{
  static unsigned
  convert_to_global_irq(unsigned type, unsigned val)
  {
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

public:
  static unsigned
  get_arm_gic_irq(Dt::Node node, unsigned idx)
  {
    if (node.has_prop("interrupts"))
      {
        Dt::Array_prop<3> ints = node.get_prop_array("interrupts", { 1, 1, 1 });

        if (idx >= ints.elements())
          return ~0u;

        return convert_to_global_irq(ints.get(idx, 0), ints.get(idx, 1));
      }

    if (node.has_prop("interrupts-extended"))
      {
        Dt::Array_prop<4> ints_ext
          = node.get_prop_array("interrupts-extended", { 1, 1, 1, 1 });

        if (idx >= ints_ext.elements())
          return ~0u;

        return convert_to_global_irq(ints_ext.get(idx, 1), ints_ext.get(idx, 2));
      }

    return ~0u;
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
