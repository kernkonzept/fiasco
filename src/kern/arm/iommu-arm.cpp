INTERFACE [iommu]:

#include "cxx/cxx_int"
#include "cxx/static_vector"
#include "tlbs.h"
#include "mmio_register_block.h"

/**
 * Common interface for all the different SMMU variations.
 */
class Iommu : public Tlb
{
public:
  // Disallow copying.
  Iommu(Iommu const &) = delete;
  Iommu &operator = (Iommu const &) = delete;

  enum { Max_iommus = CONFIG_ARM_IOMMU_MAX };
  using Iommu_array = cxx::static_vector<Iommu>;
  static Iommu *iommu(Unsigned16 iommu_idx);
  static Iommu_array &iommus() { return _iommus; }

  unsigned idx() const
  { return this - _iommus.begin(); }

private:
  /// Platform specific IOMMU initialization.
  static bool init_platform();
  /// Common IOMMU initialization.
  static void init_common();
  static Iommu_array _iommus;

private:
  enum class Rs;

  enum class Reg_access {
    Atomic,
    Non_atomic,
  };

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

    template<Reg_access ACCESS>
    static T read(Mmio_register_block const &base, unsigned index = 0)
    {
      auto reg = base.r<REG>(OFFSET + index * STRIDE);
      if constexpr (ACCESS == Reg_access::Atomic)
        return from_raw(reg.read());
      else
        return from_raw(reg.read_non_atomic());
    }

    REG raw = 0;
  };

  template<typename T, Rs RS, Address OFFSET, typename REG = Unsigned32,
           unsigned STRIDE = sizeof(REG)>
  struct Smmu_reg : public Smmu_reg_ro<T, RS, OFFSET, REG, STRIDE>
  {
    template<Reg_access ACCESS>
    void write(Mmio_register_block &base, unsigned index = 0)
    {
      auto reg = base.r<REG>(OFFSET + index * STRIDE);
      if constexpr (ACCESS == Reg_access::Atomic)
        return reg.write(this->raw);
      else
        return reg.write_non_atomic(this->raw);
    }
  };

  template<typename REG, Reg_access ACCESS = Reg_access::Atomic>
  REG read_reg(unsigned index = 0)
  {
    return REG::template read<ACCESS>(mmio_for_reg_space(REG::reg_space()), index);
  }

  template<typename REG, Reg_access ACCESS = Reg_access::Atomic>
  void write_reg(REG reg, unsigned index = 0)
  {
    return reg.template write<ACCESS>(mmio_for_reg_space(REG::reg_space()), index);
  }

  template<typename REG, Reg_access ACCESS = Reg_access::Atomic>
  void write_reg(typename REG::Val_type value)
  { return write_reg<REG, ACCESS>(REG::from_raw(value)); }
};

// ------------------------------------------------------------------
INTERFACE [iommu && arm_iommu_coherent]:

EXTENSION class Iommu { public: enum { Coherent = 1 }; };

// ------------------------------------------------------------------
INTERFACE [iommu && !arm_iommu_coherent]:

EXTENSION class Iommu { public: enum { Coherent = 0 }; };

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && dt]:

#include "dt.h"
#include "kmem_mmio.h"

PRIVATE static
bool
Iommu::init_platform_dt()
{
  unsigned i = 0;
  Dt::nodes_by_compatible("arm,smmu-v3", [&](Dt::Node) { ++i; });

  printf("Number of arm,smmu-v3: %d\n", i);

  if (i == 0)
    return false;

  _iommus = Iommu_array(new Boot_object<Iommu>[i], i);

  i = 0;
  Dt::nodes_by_compatible("arm,smmu-v3", [&](Dt::Node n)
    {
      int eventq_irq = Dt::get_arm_gic_irq(n, "eventq");
      int gerror_irq = Dt::get_arm_gic_irq(n, "gerror");

      uint64_t base, size;
      bool ret = n.get_reg(0, &base, &size);
      if (ret)
        {
          _iommus[i].setup(Kmem_mmio::map(base, size),
                           eventq_irq, gerror_irq);
          ++i;
        }
    });

  return true;
}

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && !dt]:

PRIVATE static
bool
Iommu::init_platform_dt()
{ return false; }

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && arm_acpi]:

#include "acpi.h"
#include "kmem.h"
#include "panic.h"

PRIVATE static
bool
Iommu::init_platform_acpi()
{
  auto *iort = Acpi::find<Acpi_iort const *>("IORT");
  if (!iort)
    {
      WARNX(Error, "SBSA: no IORT found!\n");
      return false;
    }

  unsigned num = 0;
  for (auto const &node : *iort)
    {
      if (node->type == Acpi_iort::Node::Smmu_v2)
        panic("SBSA: SMMUv2 not supported!");

      if (node->type == Acpi_iort::Node::Smmu_v3)
        num++;
    }

  _iommus = Iommu_array(new Boot_object<Iommu>[num], num);

  unsigned i = 0;
  for (auto const &node : *iort)
    {
      if (node->type != Acpi_iort::Node::Smmu_v3)
        continue;

      auto const *smmu = static_cast<Acpi_iort::Smmu_v3 const *>(node);
      void *v = Kmem_mmio::map(smmu->base_addr, 0x100000);
      _iommus[i++].setup(v, smmu->gsiv_event, smmu->gsiv_gerr);
    }

  return true;
}

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && !arm_acpi]:

PRIVATE static
bool
Iommu::init_platform_acpi()
{ return false; }

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && (dt || arm_acpi)]:

#include "dt.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  if (Dt::have_fdt())
    return init_platform_dt();
  return init_platform_acpi();
}

// ------------------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "static_init.h"
#include <cstdio>

Iommu::Iommu_array Iommu::_iommus;

IMPLEMENT_DEFAULT
Iommu*
Iommu::iommu(Unsigned16 iommu_idx)
{ return iommu_idx < iommus().size() ? &iommus()[iommu_idx] : nullptr; }

IMPLEMENT_DEFAULT
void
Iommu::init_common()
{}

PUBLIC static
void
Iommu::init()
{
  printf("IOMMU: Initialize\n");

  init_platform();
  if (_iommus.size() > Max_iommus)
    panic("Platform provided too many IOMMUs (%u)!", _iommus.size());

  init_common();
}

STATIC_INITIALIZE_P(Iommu, IOMMU_INIT_PRIO);
