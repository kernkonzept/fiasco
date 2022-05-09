INTERFACE [iommu]:

#include "cxx/cxx_int"
#include "cxx/static_vector"
#include "tlbs.h"

/**
 * Common interface for all the different SMMU variations.
 */
class Iommu : public Tlb
{
public:
  // Disallow copying.
  Iommu(Iommu const &) = delete;
  Iommu &operator = (Iommu const &) = delete;

  enum { Num_iommus = CONFIG_ARM_IOMMU_NUM };
  using Iommu_array = cxx::array<Iommu, unsigned, Num_iommus>;
  static Iommu *iommu(Unsigned16 iommu_idx);
  static Iommu_array &iommus() { return *_iommus.get(); }

private:
  /// Platform specific IOMMU initialization.
  static bool init_platform();
  /// Common IOMMU initialization.
  static void init_common();
  static Static_object<Iommu_array> _iommus;

private:
  enum class Rs;

  template<typename T, Rs RS, Address OFFSET, typename REG = Unsigned32,
           unsigned STRIDE = sizeof(REG)>
  struct Smmu_reg_ro
  {
    using Val_type = REG;

    static Rs reg_space()
    { return RS; }

    static T from_raw(REG raw)
    {
      T r;
      r.raw = raw;
      return r;
    }

    static T read(Mmio_register_block const &base, unsigned index = 0)
    { return from_raw(base.r<REG>(OFFSET + index * STRIDE)); }

    REG raw = 0;
  };

  template<typename T, Rs RS, Address OFFSET, typename REG = Unsigned32,
           unsigned STRIDE = sizeof(REG)>
  struct Smmu_reg : public Smmu_reg_ro<T, RS, OFFSET, REG, STRIDE>
  {
    void write(Mmio_register_block &base, unsigned index = 0)
    { base.r<REG>(OFFSET + index * STRIDE) = this->raw; }
  };

  template<typename REG>
  REG read_reg(unsigned index = 0)
  { return REG::read(mmio_for_reg_space(REG::reg_space()), index); }

  template<typename REG>
  void write_reg(REG reg, unsigned index = 0)
  { return reg.write(mmio_for_reg_space(REG::reg_space()), index); }

  template<typename REG>
  void write_reg(typename REG::Val_type value)
  { return write_reg(REG::from_raw(value)); }
};

// ------------------------------------------------------------------
INTERFACE [iommu && arm_iommu_coherent]:

EXTENSION class Iommu { public: enum { Coherent = 1 }; };

// ------------------------------------------------------------------
INTERFACE [iommu && !arm_iommu_coherent]:

EXTENSION class Iommu { public: enum { Coherent = 0 }; };

// ------------------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "static_init.h"

Static_object<Iommu::Iommu_array> Iommu::_iommus;

IMPLEMENT_DEFAULT
Iommu*
Iommu::iommu(Unsigned16 iommu_idx)
{ return iommu_idx < Num_iommus ? &iommus()[iommu_idx] : nullptr; }

IMPLEMENT_DEFAULT
void
Iommu::init_common()
{}

PUBLIC static
void
Iommu::init()
{
  printf("IOMMU: Initialize\n");
  _iommus.construct();
  init_platform();
  init_common();
}

STATIC_INITIALIZE_P(Iommu, IOMMU_INIT_PRIO);
