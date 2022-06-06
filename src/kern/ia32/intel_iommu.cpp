INTERFACE:

#include "types.h"
#include "acpi_dmar.h"
#include "cxx/bitfield"
#include "cxx/type_traits"
#include "cxx/protected_ptr"
#include "cxx/static_vector"
#include "mem.h"
#include "pm.h"
#include "tlbs.h"
#include "mem_unit.h"
#include "processor.h"

namespace Intel {

class Io_mmu :
  public Tlb,
  public Pm_object
{
public:
  /// Command and status register bits
  enum Cmd_bits
  {
    Cmd_te    = 1U << 31, ///< Translation enable
    Cmd_srtp  = 1U << 30, ///< Set root-table pointer
    Cmd_sfl   = 1U << 29, ///< Set fault log pointer
    Cmd_eafl  = 1U << 28, ///< Enable advanced fault logging
    Cmd_wbf   = 1U << 27, ///< Request write-buffer flush
    Cmd_qie   = 1U << 26, ///< Queued invalidation enable
    Cmd_ire   = 1U << 25, ///< Enable IRQ remapping
    Cmd_sirtp = 1U << 24, ///< Set IRQ remapping-table pointer
    Cmd_cfi   = 1U << 23, ///< Compatibility format IRQs
  };

  /// IRQ remapping-table entry
  struct Irte
  {
    Unsigned64 low;
    Unsigned64 hi;

    /// Present bit
    CXX_BITFIELD_MEMBER( 0,  0, present, low);
    /// disable fault processing
    CXX_BITFIELD_MEMBER( 1,  1, fpd, low);
    /// destination mode: 0 physical, 1 logical
    CXX_BITFIELD_MEMBER_UNSHIFTED( 2,  2, dm, low);
    /// redirection hint: 0 fixed target, 1 1 of N target
    CXX_BITFIELD_MEMBER_UNSHIFTED( 3,  3, rh, low);
    /// trigger mode: 0 edge, 1 level
    CXX_BITFIELD_MEMBER( 4,  4, tm, low);
    /// delivery mode: 0 fixed...
    CXX_BITFIELD_MEMBER_UNSHIFTED( 5,  7, dlm, low);
    /// software fields
    CXX_BITFIELD_MEMBER( 8, 11, sw, low);

    /// IRTE mode: 0 remapped 1: posted
    CXX_BITFIELD_MEMBER_UNSHIFTED(15, 15, im, low);
    /// target IRQ vector
    CXX_BITFIELD_MEMBER(16, 23, vector, low);
    /// destination CPU
    CXX_BITFIELD_MEMBER(32, 63, dst, low);
    CXX_BITFIELD_MEMBER(40, 47, dst_xapic, low);

    /// source identifier
    CXX_BITFIELD_MEMBER( 0, 15, sid, hi);
    /// source qualifier
    CXX_BITFIELD_MEMBER(16, 17, sq, hi);
    /// source validation type
    CXX_BITFIELD_MEMBER(18, 19, svt, hi);
    /// sid, sq, and svt in one 20bit value
    CXX_BITFIELD_MEMBER( 0, 19, src_info, hi);

    Irte() : low(0), hi(0) {}
    Irte(Irte volatile const &o) : low(o.low), hi(o.hi) {}
    Irte(Irte const &) = default;

    void clear() volatile
    {
      low = 0ULL;
      hi = 0ULL;
    }

    void operator = (Irte const &v) volatile
    {
      low = 0;
      hi = v.hi;
      low = v.low;
    }
  };

  /// Root table entry
  struct Rte
  {
    Unsigned64 _e;
    Unsigned64 _rsvd;

    Rte() : _e(0), _rsvd(0) {}

    /// Present bit
    CXX_BITFIELD_MEMBER( 0,  0, present, _e);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 63, ctp, _e);
  };

  /// Context table entry
  struct Cte
  {
    Unsigned64 _q[2];

    Cte() : _q{0, 0} {}
    Cte(Cte const &) = default;
    Cte &operator = (Cte const &) = default;

    /// Present bit
    CXX_BITFIELD_MEMBER( 0,  0, present, _q[0]);
    CXX_BITFIELD_MEMBER( 1,  1, fpd, _q[0]);
    CXX_BITFIELD_MEMBER( 2,  3, tt, _q[0]);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 63, slptptr, _q[0]);
    CXX_BITFIELD_MEMBER( 0,  2, aw, _q[1]);
    CXX_BITFIELD_MEMBER( 3,  6, os, _q[1]);
    CXX_BITFIELD_MEMBER( 8, 23, did, _q[1]);

    enum Tt
    {
      Tt_translate_all = 0,
      Tt_passthrough   = 2
    };

    typedef cxx::Protected<Cte, true> Ptr;
  };

  // IO-MMU code
  enum class Reg_32
  {
    Version                   = 0,
    Global_cmd                = 0x18,
    Global_status             = 0x1c,
    Fault_status              = 0x34,
    Fault_event_ctl           = 0x38,
    Fault_event_data          = 0x3c,
    Fault_event_addr          = 0x40,
    Fault_event_upper_addr    = 0x44,
    Protected_mem_enable      = 0x64,
    Protected_low_memy_base   = 0x68,
    Protected_low_mem_limit   = 0x6c,
    Inv_completion_status     = 0x9c,
    Inv_completion_event_ctl  = 0xa0,
    Inv_completion_event_data = 0xa4,
    Inv_completion_event_addr = 0xa8,
    Inv_completion_event_upper_addr = 0xac,
    Inv_q_error               = 0xb0,
  };

  enum class Reg_64
  {
    Capabilities     = 0x08,
    Ext_capabilities = 0x10,
    Root_table_addr  = 0x20,
    Inv_q_head       = 0x80,
    Inv_q_tail       = 0x88,
    Inv_q_addr       = 0x90,
    Irt_addr         = 0xb8
  };

  struct Registers
  {
    Address va;
    volatile Unsigned64 &operator [] (Reg_64 index)
    { return *(Unsigned64 volatile *)(va + (unsigned)index); }

    volatile Unsigned32 &operator [] (Reg_32 index)
    { return *(Unsigned32 volatile *)(va + (unsigned)index); }
  };

  enum
  {
    Fault_status_iqe = 1 << 4,
    Fault_status_ice = 1 << 5,
    Fault_status_ite = 1 << 6,

    Fault_status_mask = Fault_status_iqe | Fault_status_ice | Fault_status_ite,

    Inv_q_error_iqei = 0xf,
  };

  /// Invalidation descriptor
  struct Inv_desc
  {
    Unsigned64 l, h;
    explicit Inv_desc(Unsigned64 l = 0, Unsigned64 h = 0) : l(l), h(h) {}
    CXX_BITFIELD_MEMBER( 0,  3, type, l);

    // type 0x04 IEC
    CXX_BITFIELD_MEMBER( 4,  4, iec_g, l);
    CXX_BITFIELD_MEMBER(27, 31, im, l)
    CXX_BITFIELD_MEMBER(32, 47, iidx, l);

    // type 0x01 CCI
    CXX_BITFIELD_MEMBER( 4,  5, cc_g, l);
    CXX_BITFIELD_MEMBER(16, 31, cc_did, l);
    CXX_BITFIELD_MEMBER(32, 47, cc_src_id, l);
    CXX_BITFIELD_MEMBER(48, 49, cc_fm, l);

    // type 0x02 IO-TLB
    CXX_BITFIELD_MEMBER( 4,  5, io_g, l);
    CXX_BITFIELD_MEMBER( 6,  6, io_dw, l);
    CXX_BITFIELD_MEMBER( 7,  7, io_dr, l);
    CXX_BITFIELD_MEMBER(16, 31, io_did, l);
    CXX_BITFIELD_MEMBER( 0,  5, io_am, h);
    CXX_BITFIELD_MEMBER( 6,  6, io_ih, h);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 63, io_aadr, h);

    // type 0x05 WAIT
    CXX_BITFIELD_MEMBER( 4,  4, wait_if, l);
    CXX_BITFIELD_MEMBER( 5,  5, wait_sw, l);
    CXX_BITFIELD_MEMBER( 6,  6, wait_fn, l);
    CXX_BITFIELD_MEMBER(32, 63, wait_data, l);
    CXX_BITFIELD_MEMBER_UNSHIFTED( 2, 63, wait_addr, h);


    /// Global IRQ remapping invalidation
    static Inv_desc global_iec() { return Inv_desc(0x4); }
    static Inv_desc iec(unsigned index, unsigned im = 0)
    {
      return Inv_desc(0x14 | im_bfm_t::val_dirty(im)
                      | iidx_bfm_t::val_dirty(index));
    }

    /**
     * Wait for completion of preceeding invalidation descriptors.
     *
     * \param flag_phys_addr  Physical address of a 32-bit wait flag,
     *                        will be set to 0 when completed.
     */
    static Inv_desc wait_phys(Address flag_phys_addr, bool fence = false)
    { return Inv_desc(0x25 | wait_fn_bfm_t::val(fence), flag_phys_addr); }

    /**
     * Wait for completion of preceeding invalidation descriptors.
     *
     * \param flag  Wait flag located in pmem, will be set to 0 when completed.
     */
    static Inv_desc wait_pmem(Unsigned32 volatile *flag, bool fence = false)
    {
      Address flag_addr = reinterpret_cast<Address>(flag);
      assert (Mem_layout::in_pmem(flag_addr));
      return wait_phys(Mem_layout::pmem_to_phys(flag_addr), fence);
    }

    /**
     * Wait for completion of preceeding invalidation descriptors.
     *
     * \param flag  Wait flag, will be set to 0 when completed.
     */
    static Inv_desc wait_virt(Unsigned32 volatile *flag, bool fence = false)
    {
      Address flag_addr = reinterpret_cast<Address>(flag);
      return wait_phys(Kmem::kdir->virt_to_phys(flag_addr), fence);
    }

    /// global IOTLB invalidation
    static Inv_desc iotlb_glbl()
    { return Inv_desc(0x12 | io_dw_bfm_t::val(1) | io_dr_bfm_t::val(1)); }

    /// Domain-ID based IOTLB invalidation
    static Inv_desc iotlb_did(unsigned did)
    {
      return Inv_desc(0x22 | io_dw_bfm_t::val(1) | io_dr_bfm_t::val(1)
                      | io_did_bfm_t::val(did));
    }

    /// Global context-cache invalidation
    static Inv_desc cc_full()
    { return Inv_desc(0x11); }

    /// Domain-ID based context-cache invalidation
    static Inv_desc cc_did(unsigned did)
    { return Inv_desc(0x21 | cc_did_bfm_t::val(did)); }

    /// Device specific context-cache invalidation
    static Inv_desc cc_src(unsigned did, unsigned src_id, unsigned fm = 0)
    {
      return Inv_desc(0x31 | cc_did_bfm_t::val(did)
                      | cc_src_id_bfm_t::val(src_id)
                      | cc_fm_bfm_t::val(fm));
    }

    /// Device specific context-cache invalidation
    static Inv_desc cc_dev(unsigned did, unsigned bus, unsigned dev,
                           unsigned fm = 0)
    { return cc_src(did, (bus << 8) | dev, fm); }

    /// Invalidation descriptor that does nothing
    static Inv_desc nop()
    {
      // Wait descriptor without notification.
      return Inv_desc(0x5);
    }
  };

  /// Capability register
  struct Caps
  {
    Unsigned64 raw;
    CXX_BITFIELD_MEMBER( 0,  2, nd, raw);
    CXX_BITFIELD_MEMBER( 3,  3, afl, raw);
    CXX_BITFIELD_MEMBER( 4,  4, rwbf, raw);
    CXX_BITFIELD_MEMBER( 5,  5, plmr, raw);
    CXX_BITFIELD_MEMBER( 6,  6, phmr, raw);
    CXX_BITFIELD_MEMBER( 7,  7, cm, raw);
    CXX_BITFIELD_MEMBER( 8, 12, sgaw, raw);
    CXX_BITFIELD_MEMBER(16, 21, mgaw, raw);
    CXX_BITFIELD_MEMBER(22, 22, zlr, raw);
    CXX_BITFIELD_MEMBER(24, 33, fro, raw);
    CXX_BITFIELD_MEMBER(34, 37, sllps, raw);
    CXX_BITFIELD_MEMBER(39, 39, psi, raw);
    CXX_BITFIELD_MEMBER(40, 47, nfr, raw);
    CXX_BITFIELD_MEMBER(48, 53, mamv, raw);
    CXX_BITFIELD_MEMBER(54, 54, dwd, raw);
    CXX_BITFIELD_MEMBER(55, 55, drd, raw);
    CXX_BITFIELD_MEMBER(56, 56, fl1gb, raw);
    CXX_BITFIELD_MEMBER(59, 59, pi, raw);
  };

  Unsigned64 base_addr = 0;
  ACPI::Dev_scope_vect devs;

  Unsigned16 segment;
  Unsigned8  flags;

  Caps caps;
  Unsigned64 ecaps;
  Registers regs;

  Spin_lock<> _lock;

  Spin_lock<> _inv_q_lock;
  unsigned inv_q_tail = 0;
  unsigned inv_q_size = 0;
  Inv_desc *inv_q;

  Rte *_root_table = 0;

  Irte volatile *_irq_remapping_table = 0;
  unsigned _irq_remap_table_size = 0;

  Io_mmu()
  : _lock(Spin_lock<>::Unlocked), _inv_q_lock(Spin_lock<>::Unlocked)
  {}

  unsigned idx() const
  { return this - iommus.begin(); }

  Irte volatile *irq_remapping_table() const
  { return _irq_remapping_table; }

  Inv_desc *inv_desc(unsigned tail)
  { return inv_q + tail; }

  template<typename T>
  void clean_dcache(T *s, T *e)
  {
    typedef typename cxx::remove_cv<T>::type Ct;
    if (!coherent())
      Mem_unit::clean_dcache(const_cast<Ct const *>(s),
                             const_cast<Ct const *>(e));
  }

  template<typename T>
  void clean_dcache(T *s)
  {
    typedef typename cxx::remove_cv<T>::type Ct;
    if (!coherent())
      Mem_unit::clean_dcache(const_cast<Ct const *>(s),
                             const_cast<Ct const *>(s) + 1);
  }


  Unsigned32 modify_cmd(Unsigned32 set, Unsigned32 clear = 0)
  {
    Unsigned32 v = regs[Reg_32::Global_status];
    v &= ~clear;
    v |= set;
    regs[Reg_32::Global_cmd] = v;
    return v;
  }

  bool probe(ACPI::Dmar_drhd const *drhd);
  void setup(Cpu_number cpu);

  FIASCO_INIT
  void set_irq_remapping_table(Irte *irt, Unsigned64 irt_pa, unsigned order)
  {
    _irq_remapping_table = irt;
    _irq_remap_table_size = order;
    regs[Reg_64::Irt_addr] = irt_pa | (order - 1);
    modify_cmd(Cmd_sirtp);
    queue_and_wait<false>(Inv_desc::global_iec());
    modify_cmd(Cmd_ire, Cmd_cfi);
  }

  void pm_on_resume(Cpu_number) override;
  void pm_on_suspend(Cpu_number) override {}

  void tlb_flush() override;

  /**
   * Detect and handle error conditions of the invalidation queue.
   *
   * \pre `_inv_q_lock` must not be held.
   */
  void queue_check_error()
  {
    bool error_pending = regs[Reg_32::Fault_status] & Fault_status_mask;
    if (EXPECT_TRUE(!error_pending))
      return;

    // Handle errors with the queue lock held, then release it before reporting
    // the errors.
    Unsigned32 queue_errors;
    {
      auto g = lock_guard(_inv_q_lock);
      queue_errors = regs[Reg_32::Fault_status];

      // Invalidation Queue Error
      if (queue_errors & Fault_status_iqe)
        {
          // Head register points to the descriptor associated with the
          // invalidation queue error.
          Inv_desc *desc = inv_desc(regs[Reg_64::Inv_q_head] / sizeof(Inv_desc));

          // Replace the failing command with a nop descriptor.
          write_now(desc, Inv_desc::nop());
        }

      // NOTE: Invalidation Completion and Invalidation Time-out errors should
      //       never occur, because we do not use device TLBs.

      // Clear errors.
      regs[Reg_32::Fault_status] = queue_errors & Fault_status_mask;
    }

    WARN("IOMMU: Invalidation queue error(s): 0x%x\n", queue_errors);
  }

  /**
   * Return the number of free slots in the invalidation queue.
   *
   * \pre `_inv_q_lock` must be held.
   */
  unsigned queue_num_free_slots()
  {
      unsigned hw_head = regs[Reg_64::Inv_q_head] / sizeof(Inv_desc);
      return (hw_head - inv_q_tail - 1u) & inv_q_size;
  }

  /**
   * Submit invalidation descriptors to the invalidation queue.
   *
   * \param  descs  Invalidation descriptors to submit.
   *
   * \pre `sizeof...(descs)` < `inv_q_size`
   */
  template<typename... Inv_descs>
  void invalidate(Inv_descs... descs)
  {
    constexpr unsigned desc_num = sizeof...(descs);
    Inv_desc desc_array[desc_num] = {descs...};

    auto g = lock_guard(_inv_q_lock);

    // Wait until the queue has at least desc_num free slots.
    for(;;)
      {
        // Check if queue has enough free slots.
        if (EXPECT_TRUE(desc_num <= queue_num_free_slots()))
          break;

        // Release lock.
        g.reset();

        // Handle queue errors to ensure that the queue makes progress.
        queue_check_error();

        // Queue is full, wait for free entries.
        // TODO: We might want to add a preemption point here!
        Proc::pause();

        // Reacquire lock.
        g.lock(&_inv_q_lock);
      }

    // Put descriptors into queue.
    for (unsigned i = 0; i < desc_num; i++)
      {
        *inv_desc(inv_q_tail) = desc_array[i];
        inv_q_tail = (inv_q_tail + 1) & inv_q_size;
      }

    // Force compiler to write descriptors before updating tail pointer.
    Mem::barrier();

    // Update the hardware tail pointer.
    regs[Reg_64::Inv_q_tail] = inv_q_tail * sizeof(Inv_desc);
  }

  /**
   * Submit the given invalidation descriptors and wait for their completion.
   *
   * \tparam Stack_in_pmem  Whether this code is executed with a stack that is
   *                        allocated from pmem. Only relevant during initial
   *                        IOMMU setup phase where the kernel executes with a
   *                        pre-allocated stack not located in pmem.
   *
   * \param descs  Invalidation descriptors to submit.
   */
  template<bool Stack_in_pmem = true, typename... Inv_descs>
  void queue_and_wait(Inv_descs... descs)
  {
    Unsigned32 volatile flag = 1;
    invalidate(descs..., Stack_in_pmem ? Inv_desc::wait_pmem(&flag)
                                       : Inv_desc::wait_virt(&flag));

    while (flag)
      {
        // Handle queue errors to ensure that the queue makes progress.
        queue_check_error();

        // TODO: We might want to add a preemption point here!
        Proc::pause();
      }
  }

  /**
   * Submit the given invalidation descriptors on all IOMMUs for which the need
   * for invalidation is indicated and wait for their completion.
   *
   * \param need_inv    Array which indicates for each IOMMU whether it shall
   *                    participate in the invalidation and wait.
   * \param descs       Invalidation descriptors to submit.
   */
  template<typename... Inv_descs>
  static void queue_and_wait_on_iommus(bool const *need_inv, Inv_descs... descs)
  {
    Unsigned32 volatile wait_flags[iommus.size()];
    for (unsigned i = 0; i < iommus.size(); i++)
      {
        // Skip if this IOMMU does not need invalidation.
        if (need_inv != nullptr && !need_inv[i])
          {
            wait_flags[i] = 0;
            continue;
          }

        wait_flags[i] = 1;
        iommus[i].invalidate(descs..., Inv_desc::wait_pmem(&wait_flags[i]));
      }

    for(;;)
      {
        bool any_wait = false;
        for (unsigned i = 0; i < iommus.size(); i++)
          {
            if (wait_flags[i])
              {
                // This IOMMU has not yet completed the wait descriptor.
                any_wait = true;

                // Handle queue errors to ensure that the queue makes progress.
                iommus[i].queue_check_error();
              }
          }

        if (any_wait)
          // TODO: We might want to add a preemption point here!
          Proc::pause();
        else
          // All IOMMUs are done executing.
          break;
      };
  }

  /**
   * Submit the given invalidation descriptors on all IOMMUs and wait for their
   * completion.
   *
   * \param descs  Invalidation descriptors to submit.
   */
  template<typename... Inv_descs>
  static void queue_and_wait_on_all_iommus(Inv_descs... descs)
  { return queue_and_wait_on_iommus(nullptr, descs...); }

  enum class Flush_op
  {
    No_flush,
    Flush,
    Flush_and_wait,
  };

  void set_irq_mapping(Irte const &irte, unsigned index, Flush_op flush)
  {
    _irq_remapping_table[index] = irte;
    clean_dcache(&_irq_remapping_table[index]);

    Inv_desc inv_desc = Inv_desc::iec(index);
    switch (flush)
      {
      case Flush_op::No_flush: break;
      case Flush_op::Flush: invalidate(inv_desc); break;
      case Flush_op::Flush_and_wait: queue_and_wait(inv_desc); break;
      };
  }

  Irte get_irq_mapping(unsigned index)
  {
    return _irq_remapping_table[index];
  }

  // static data
  typedef cxx::static_vector<Io_mmu> Io_mmu_vect;

  static Io_mmu_vect iommus;
  static Acpi_dmar::Flags dmar_flags;
  static unsigned hw_addr_width;
  static FIASCO_INIT bool init(Cpu_number cpu);

  /// Are 4 level page tables supported ?
  bool use_4lvl_pt() const
  { return caps.sgaw() & (1 << 2); }

  /**
   * Supported Address-Width ID
   * \retval 1  3 level page tables
   * \retval 2  4 level page tables
   */
  unsigned aw() const
  { return use_4lvl_pt() + 1; }

  /// Is this IOMMU cache coherent?
  bool coherent() const { return ecaps & 1; }

  /// Is this IOMMU the default PCI MMU?
  bool is_default_pci() const { return flags & 1; }

  /**
   * Get a pointer to the context entry for the given device.
   * \param bus        PCI bus number.
   * \param df         Device + Function ID (device << 3 | function).
   * \param may_alloc  set to true if new context tables shall be
   *                   allocated.
   */
  Cte::Ptr get_context_entry(Unsigned8 bus, Unsigned8 df, bool may_alloc);

  bool _flush_context_entry(Cte::Ptr entry, Unsigned8 bus,
                           Unsigned8 df, Cte old, Cte new_cte)
  {
    // If neither entry is present we can skip all flushes.
    if (!new_cte.present() && !old.present())
      return false;

    // If either the new or the old entry is present we need a cache flush.
    clean_dcache(entry.unsafe_ptr());

    // Flush the IOMMU caches:
    //   1. if the old entry was present.
    //   2. if the old entry was not present but the new entry is and the IOMMU
    //      might cache invalid entries (caps.cm() is true).
    if (old.present() || (caps.cm() && new_cte.present()))
      {
        // If the IOMMU caches a non-present context entry, the cache entry is
        // tagged with the reserved domain id 0. When invalidating context-table
        // entries we set them to all zeroes, thus also for non-present entries
        // passing old.did() as the domain id for the context-cache invalidation
        // descriptor is correct.
        Inv_desc inv_cc = Inv_desc::cc_dev(old.did(), bus, df);
        if (old.did())
          invalidate(inv_cc, Inv_desc::iotlb_did(old.did()));
        else
          invalidate(inv_cc);
        return true;
      }

    return false;
  }

  /**
   * Set the context entry for the given device.
   *
   * \param      entry    Pointer to the context entry.
   * \param      bus      PCI bus number.
   * \param      df       Device + Function ID (device << 3 | function).
   * \param      new_cte  New context entry to set.
   *
   * \return Whether invalidation descriptors were submitted to the
   *         invalidation queue.
   *
   * \note Waiting for the execution of invalidation descriptors submitted
   *       according to `flushed` to complete is the responsibility of the
   *       caller of this method.
   */
  bool set_context_entry(Cte::Ptr entry, Unsigned8 bus,
                         Unsigned8 df, Cte new_cte)
  {
    if (EXPECT_FALSE(!entry))
      return false;

    Cte old;

      {
        auto g = lock_guard(_lock);
        old = entry.get();
        write_consistent(entry.unsafe_ptr(), new_cte);
      }

    return _flush_context_entry(entry, bus, df, old, new_cte);
  }

  /**
   * Compare and swap the context entry for the given device.
   *
   * \param      entry    Pointer to the context entry.
   * \param      bus      PCI bus number.
   * \param      df       Device + Function ID (device << 3 | function).
   * \param      old      Old context entry which is compared before setting
   *                      the new context entry.
   * \param      new_cte  New context entry to set.
   * \param[out] flushed  Set to true if invalidation descriptors were submitted
   *                      to the invalidation queue.
   *
   * \return Whether the context entry was set to the new value.
   *
   * \note Waiting for the execution of invalidation descriptors submitted
   *       according to `flushed` to complete is the responsibility of the
   *       caller of this method.
   */
  bool cas_context_entry(Cte::Ptr entry, Unsigned8 bus,
                         Unsigned8 df, Cte old, Cte new_cte, bool *flushed)
  {
      {
        auto g = lock_guard(_lock);
        if (old._q[0] != entry->_q[0]
            || old._q[1] != entry->_q[1])
          return false;
        write_consistent(entry.unsafe_ptr(), new_cte);
      }

    *flushed |= _flush_context_entry(entry, bus, df, old, new_cte);
    return true;
  }

  unsigned num_domains() const
  { return 1U << ((caps.nd() * 2) + 4); }

  void allocate_root_table();
};

}

// ------------------------------------------------------------------
IMPLEMENTATION:

#include "apic.h"
#include "acpi_dmar.h"
#include <cstdio>
#include "warn.h"

Intel::Io_mmu::Io_mmu_vect Intel::Io_mmu::iommus;
Acpi_dmar::Flags Intel::Io_mmu::dmar_flags;
unsigned Intel::Io_mmu::hw_addr_width;

enum { Print_infos = 0 };

IMPLEMENT
bool
Intel::Io_mmu::probe(ACPI::Dmar_drhd const *drhd)
{
  base_addr = drhd->register_base;
  devs      = drhd->devs();
  segment   = drhd->segment;
  flags     = drhd->flags;

  Address va = Kmem::mmio_remap(base_addr, Config::PAGE_SIZE);

  Kip::k()->add_mem_region(Mem_desc(base_addr, base_addr + Config::PAGE_SIZE -1,
                                    Mem_desc::Reserved));
  regs.va = va;

  caps.raw = regs[Reg_64::Capabilities];
  ecaps = regs[Reg_64::Ext_capabilities];

  if (Print_infos)
    printf("IOMMU: %llx va=%lx version=%x caps=%llx:%llx\n",
           base_addr, va, regs[Reg_32::Version], caps.raw, ecaps);

  if (caps.rwbf())
    {
      WARN("IOMMU: cannot handle IOMMUs that need write-buffer flushes\n");
      return false;
    }

  if (!(ecaps & (1 << 1)))
    {
      WARN("IOMMU: queued invalidation not supported, will not use IOMMU\n");
      return false;
    }

  return true;
}

IMPLEMENT
void
Intel::Io_mmu::setup(Cpu_number cpu)
{
  inv_q_size = 256 - 1;
  inv_q = Kmem_alloc::allocator()->alloc_array<Inv_desc>(inv_q_size + 1);
  Address inv_q_pa = Kmem::virt_to_phys(inv_q);

  regs[Reg_64::Inv_q_tail] = inv_q_tail * sizeof(Inv_desc);
  regs[Reg_64::Inv_q_addr] = inv_q_pa;
  modify_cmd(Cmd_qie);

  register_pm(cpu);
  register_iommu_tlb();
}

IMPLEMENT FIASCO_INIT
bool
Intel::Io_mmu::init(Cpu_number cpu)
{
  dmar_flags.raw = 0;
  auto d = Acpi::find<Acpi_dmar const *>("DMAR");
  // no DMAR -> no IRQ remapping, fall back to normal IO APIC
  if (!d)
    return false;

  dmar_flags = d->flags;
  // the value reported in the DMA remapping reporting structure is N but the
  // host address width has to be computed as (N + 1)
  hw_addr_width = d->haw + 1;

  // first count the units
  unsigned units = 0;
  for (ACPI::Dmar_head const &de: *d)
    if (de.cast<ACPI::Dmar_drhd>())
      ++units;

  // need to take a copy of the DMAR into the kernel AS as the ACPI
  // are mapped only into IDLE AS!
  void *dmar = Boot_alloced::alloc(d->len);
  memcpy(dmar, d, d->len);
  d = reinterpret_cast<Acpi_dmar *>(dmar);

  iommus = Io_mmu_vect(new Boot_object<Io_mmu>[units], units);

  units = 0;
  for (ACPI::Dmar_head const &de: *d)
    if (ACPI::Dmar_drhd const *i = de.cast<ACPI::Dmar_drhd>())
      {
        if (!iommus[units++].probe(i))
          {
            // Probing the IOMMU failed, not using IOMMUs.
            dmar_flags.raw = 0;
            iommus = Io_mmu_vect();
            return false;
          }

        if (i->segment != 0)
          WARN("IOMMU: no proper support for PCI Segment Groups\n");
      }

  for (auto &iommu: Intel::Io_mmu::iommus)
    iommu.setup(cpu);

  return true;
}

IMPLEMENT
void
Intel::Io_mmu::allocate_root_table()
{
  Rte *root_table = Kmem_alloc::allocator()->alloc_array<Rte>(256);
  assert (root_table);
  Address root_table_pa = Kmem::virt_to_phys(root_table);
  _root_table = root_table;

  regs[Reg_64::Root_table_addr] = root_table_pa;
  modify_cmd(Cmd_srtp);
}


IMPLEMENT
void
Intel::Io_mmu::pm_on_resume(Cpu_number cpu)
{
  // first enable invalidation queue
  Address inv_q_pa = Kmem::virt_to_phys(inv_q);
  inv_q_tail = 0;
  regs[Reg_64::Inv_q_tail] = inv_q_tail * sizeof(Inv_desc);
  regs[Reg_64::Inv_q_addr] = inv_q_pa;
  modify_cmd(Cmd_qie);

  if (_irq_remapping_table)
    {
      Mword target =  Apic::apic.cpu(cpu)->cpu_id();

      for (auto *irte = _irq_remapping_table;
           irte != _irq_remapping_table + (1 << _irq_remap_table_size);
           ++irte)
        {
          Intel::Io_mmu::Irte e = *irte;
          if (!e.present() || target == e.dst_xapic())
            continue;

          e.dst_xapic() = target;
          *irte = e;
        }

      Address irt_pa = Kmem::virt_to_phys(const_cast<Irte*>(_irq_remapping_table));
      regs[Reg_64::Irt_addr] = irt_pa | (_irq_remap_table_size - 1);
      modify_cmd(Cmd_sirtp);
      queue_and_wait(Inv_desc::global_iec());
      modify_cmd(Cmd_ire, Cmd_cfi);
    }

  if (_root_table)
    {
      Address root_table_pa = Kmem::virt_to_phys(_root_table);

      regs[Reg_64::Root_table_addr] = root_table_pa;
      modify_cmd(Cmd_srtp);
      queue_and_wait(Inv_desc::cc_full(), Inv_desc::iotlb_glbl());
      modify_cmd(Cmd_te);
    }
}

IMPLEMENT
void
Intel::Io_mmu::tlb_flush()
{
  queue_and_wait(Inv_desc::iotlb_glbl());
}

IMPLEMENT
Intel::Io_mmu::Cte::Ptr
Intel::Io_mmu::get_context_entry(Unsigned8 bus, Unsigned8 df, bool may_alloc)
{
  cxx::Protected<Rte, true> rte = &_root_table[bus];

  if (rte->present())
    return ((Cte *)Mem_layout::phys_to_pmem(rte->ctp())) + df;

  if (EXPECT_FALSE(!may_alloc))
    return 0;

  enum { Ct_size = 4096 };
  const Bytes Ct_bytes = Bytes(Ct_size);
  void *ctx = Kmem_alloc::allocator()->alloc(Ct_bytes);
  if (EXPECT_FALSE(!ctx))
    return 0; // out of memory

  memset(ctx, 0, Ct_size);

  Rte n = Rte();
  n.present() = 1;
  n.ctp() = Mem_layout::pmem_to_phys(ctx);

    {
      auto g = lock_guard(_lock);

      if (EXPECT_FALSE(rte->present()))
        {
          // someone else allocated the context table meanwhile
          g.reset();
          // we assume context tables are never freed
          Kmem_alloc::allocator()->free(Ct_bytes, ctx);
          return ((Cte *)Mem_layout::phys_to_pmem(rte->ctp())) + df;
        }

      write_consistent(rte.unsafe_ptr(), n);
    }

  clean_dcache(rte.unsafe_ptr());

  return ((Cte *)ctx) + df;
}
