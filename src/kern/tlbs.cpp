INTERFACE:

#include <cxx/hlist>
#include "per_cpu_data.h"
#include "mem_unit.h"

/**
 * This class serves as a registry for all TLBs in the system, apart from the
 * regular MMU TLBs.
 *
 * The TLB registry consists of a list of CPU-independent TLBs, like e.g.
 * IOMMU TLBs, and a per CPU list of CPU-dependent TLBs, like e.g. EPT TLBs.
 * During a full TLB flush, this allows us to flush not only the regular MMU
 * TLB, but also the registered special purpose TLBs.
 *
 * For each of these TLBs a handler is registered in registry, which derives
 * from the Tlb class and implements the pure virtual tlb_flush() method.
 */
class Tlb : public cxx::H_list_item
{
private:
  typedef cxx::H_list<Tlb> Tlb_list;

public:

  // flush the TLB
  virtual void tlb_flush() = 0;

  // register CPU-dependent TLBs
  void register_cpu_tlb(Cpu_number cpu)
  {
    _cpu_tlbs.cpu(cpu).push_front(this);
  }

  // register IOMMU TLB
  void register_iommu_tlb()
  {
    _iommu_tlbs.push_front(this);
  }

  // flush all CPU-dependent TLBs
  static void flush_all_cpu(Cpu_number cpu)
  {
    // flush local MMU TLB
    Mem_unit::tlb_flush();

    for (auto const &tlb: _cpu_tlbs.cpu(cpu))
      tlb->tlb_flush();
  }

  // flush all IOMMU TLBs
  static void flush_all_iommu()
  {
    for (auto const &tlb: _iommu_tlbs)
      tlb->tlb_flush();
  }

private:
  static Per_cpu<Tlb_list> _cpu_tlbs;
  static Tlb_list _iommu_tlbs;
};

IMPLEMENTATION:

DEFINE_PER_CPU Per_cpu<Tlb::Tlb_list> Tlb::_cpu_tlbs;
Tlb::Tlb_list Tlb::_iommu_tlbs;
