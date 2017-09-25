INTERFACE[amd64]:

EXTENSION class Config
{
public:

  enum {
    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1 << PAGE_SHIFT,
    PAGE_MASK  = ~( PAGE_SIZE - 1),

    SUPERPAGE_SHIFT = 21,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK  = ~( SUPERPAGE_SIZE -1 ),

    PDP_SIZE		= 1LL << 30,
    PML4_SIZE		= 1LL << 39,

    PML4E_SHIFT		= 39,
    PML4E_MASK		= 0x1ff,
    PDPE_SHIFT		= 30,
    PDPE_MASK		= 0x1ff,
    PDE_SHIFT		= 21,
    PDE_MASK		= 0x1ff,
    PTE_SHIFT		= 12,
    PTE_MASK		= 0x1ff,

    Irq_shortcut = 1,
  };
};
