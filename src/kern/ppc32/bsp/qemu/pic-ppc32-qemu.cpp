INTERFACE [ppc32 && qemu]:

EXTENSION class Pic
{
public:
  static Pic *const main;

  // XXX: Hack to for ignoring IRQs all together
  static void post_pending_irqs() {}
  static void init() {}
};

