IMPLEMENTATION [arm && cpu_virt]:

#include "feature.h"

EXTENSION class Cpu
{
public:
  enum : Unsigned32
  {
    Hcr_vm   = 1 << 0,
    Hcr_swio = 1 << 1,
    Hcr_ptw  = 1 << 2,
    Hcr_fmo = 1 << 3,
    Hcr_imo = 1 << 4,
    Hcr_amo = 1 << 5,
    Hcr_dc  = 1 << 12,
    Hcr_tsc = 1 << 19,
    Hcr_tidcp = 1 << 20,
    Hcr_tactlr = 1 << 21,
    Hcr_tge  = 1 << 27,
  };
};

KIP_KERNEL_FEATURE("arm:hyp");

//--------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && 32bit]:

EXTENSION class Cpu
{
public:
  enum : Unsigned32
  {
    Hcr_must_set_bits = Hcr_vm | Hcr_swio | Hcr_ptw
                      | Hcr_amo | Hcr_imo | Hcr_fmo
                      | Hcr_tidcp | Hcr_tsc | Hcr_tactlr,

    Hcr_host_bits = Hcr_must_set_bits | Hcr_dc | Hcr_tge
  };
};

IMPLEMENT_OVERRIDE inline void Cpu::init_mmu(bool) {}

IMPLEMENT_OVERRIDE
void
Cpu::init_hyp_mode()
{
  extern char hyp_vector_base[];

  assert (!((Mword)hyp_vector_base & 31));
  asm volatile ("mcr p15, 4, %0, c12, c0, 0 \n" : : "r"(hyp_vector_base));

  asm volatile (
        "mrc p15, 4, r0, c1, c1, 1 \n"
        "orr r0, #(0xf << 8) \n" // enable TDE, TDA, TDOSA, TDRA
        "mcr p15, 4, r0, c1, c1, 1 \n"

        "mcr p15, 4, %0, c2, c1, 2 \n"

        "mrc p15, 0, r0, c1, c0, 0 \n"
        "bic r0, #1 \n"
        "mcr p15, 0, r0, c1, c0, 0 \n"

        "mcr p15, 4, %1, c1, c1, 0 \n"
        : :
        "r" ((1UL << 31) | (Page::Tcr_attribs << 8) | (1 << 6)),
        "r" (Hcr_tge | Hcr_dc | Hcr_must_set_bits)
        : "r0" );

  Mem::dsb();
  Mem::isb();

  // HCPTR
  asm volatile("mcr p15, 4, %0, c1, c1, 2" : :
               "r" (  0x33ff       // TCP: 0-9, 12-13
                    | (1 << 20))); // TTA
}
