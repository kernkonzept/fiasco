INTERFACE [arm && mmu && arm_lpae]:

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits =   (1 << 31) // EAE
                 | (Tcr_attribs << 8),
  };
};
