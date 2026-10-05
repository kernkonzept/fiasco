INTERFACE [iommu]:

#include <cxx/cxx_int>
#include <cxx/static_vector>
#include "config.h"
#include "mmio_register_block.h"
#include "tlbs.h"
#include "warn.h"

/**
 * Common interface for all the different SMMU variations.
 *
 * Concrete SMMU drivers derive from this class. Each constructed driver
 * instance automatically registers itself in the global IOMMU registry. The
 * registration order defines the IOMMU index by which kernel and user space
 * refer to an IOMMU.
 */
class Iommu : public Tlb
{
public:
  // Disallow copying.
  Iommu(Iommu const &) = delete;
  Iommu &operator = (Iommu const &) = delete;

  static constexpr bool Debug = Config::Jdb;
  static constexpr bool Log_faults = Config::Jdb
                                     && Warn::is_enabled(Warn_level::Info);

  enum { Max_iommus = CONFIG_ARM_IOMMU_MAX };
  using Iommu_array = cxx::static_vector<Iommu *>;

  static Iommu *iommu(Unsigned16 iommu_idx)
  { return iommu_idx < _num_iommus ? _iommus[iommu_idx] : nullptr; }

  static Iommu_array iommus()
  { return Iommu_array(_iommus, _num_iommus); }

  /// Index of this IOMMU in the registry.
  unsigned idx() const
  { return _idx; }

  enum class Iommu_type : Unsigned8
  {
    Smmu_v2,
    Smmu_v3,
  };

  Iommu_type type() const
  { return _type; }

  /**
   * Whether the IOMMUs access their translation tables and queues cache
   * coherently. It starts with the default of the platform
   * (CONFIG_ARM_IOMMU_COHERENT), a driver clears it before it sets up an IOMMU
   * that the firmware does not describe as coherent.
   */
  static bool coherent()
  { return _coherent; }

protected:
  static void set_non_coherent()
  { _coherent = false; }

private:
  static bool _coherent;

  static Iommu *_iommus[Max_iommus];
  static unsigned _num_iommus;

  Unsigned16 _idx;

protected:
  Iommu_type _type;

  /// Register this IOMMU in the global registry ordered by the object
  /// construction order.
  Iommu();

  enum class Reg_access
  {
    Atomic,
    Non_atomic,
  };

  template<typename T, auto RS, Address OFFSET, typename REG = Unsigned32,
           unsigned STRIDE = sizeof(REG)>
  struct Smmu_reg_ro
  {
    using Val_type = REG;

    static auto reg_space()
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

  template<typename T, auto RS, Address OFFSET, typename REG = Unsigned32,
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
};

// ------------------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "panic.h"

constinit Iommu *Iommu::_iommus[Iommu::Max_iommus];
constinit unsigned Iommu::_num_iommus;
constinit bool Iommu::_coherent = TAG_ENABLED(arm_iommu_coherent);

IMPLEMENT
Iommu::Iommu()
{
  if (_num_iommus >= Max_iommus)
    panic("IOMMU: Platform provided too many IOMMUs (max %u)!",
          static_cast<unsigned>(Max_iommus));

  _idx = _num_iommus++;
  _iommus[_idx] = this;
}
