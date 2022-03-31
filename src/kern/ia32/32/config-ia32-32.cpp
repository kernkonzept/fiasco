INTERFACE[ia32,ux]:

EXTENSION class Config
{
public:

  enum {
    PAGE_SHIFT          = ARCH_PAGE_SHIFT,
    PAGE_SIZE           = 1 << PAGE_SHIFT,
    PAGE_MASK           = ~( PAGE_SIZE - 1),

    SUPERPAGE_SHIFT     = 22,
    SUPERPAGE_SIZE      = 1 << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK      = ~( SUPERPAGE_SIZE - 1 ),

    Irq_shortcut        = 1,

    Ext_vcpu_state_size = PAGE_SIZE,
  };
};
