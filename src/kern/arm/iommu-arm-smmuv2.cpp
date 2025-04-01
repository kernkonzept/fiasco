INTERFACE [iommu && (32bit || iommu_arm_smmu_400)]:

EXTENSION class Iommu
{
  enum
  {
    // LPAE page table format
    Va64_support   = 0,
    // 32-bit virtual address space
    Virt_addr_size = 32,
    // 40-bit physical address space
    Phys_addr_size = 40,
  };
};

INTERFACE [iommu && 64bit && iommu_arm_smmu_500]:

EXTENSION class Iommu
{
  enum
  {
    // AArch64 page table format
    Va64_support   = 1,
    // 40-bit virtual address space
    Virt_addr_size = 40,
    // 40-bit physical address space
    Phys_addr_size = 40,
  };
};

INTERFACE [iommu]:

#include "types.h"
#include "spin_lock.h"
#include "boot_alloc.h"
#include "mmio_register_block.h"
#include "cxx/bitfield"

/*
 * Iommu implementation for ARM SMMUv1 and SMMUv2.
 *
 * The mapping between a device and a page table works as follows:
 * Each device is identified by a `stream ID`. This stream ID is matched by a
 * `Stream Mapping Group`, consisting of a SMR/S2CR register pair. This Stream
 * Mapping Group points to a Translation `Context Bank`, which in turn points
 * to a page table.
 *
 * Identical stream IDs are matched by the same Stream Mapping Group.
 * Identical page tables are managed by the same Context Bank.
 *
 * The SMMU is configured, so that devices for whose stream ID no mapping
 * exists are blocked by the SMMU, and therefore cannot access physical memory
 * via DMA.
 *
 * Interrupts are only used to output a debug message and only enabled if the
 * warning level is high enough, currently Warn_level::Info.
 */
EXTENSION class Iommu
{
public:
  enum class Version
  {
    Smmu_v1,
    Smmu_v2,
  };

  using Vmid = Unsigned32;
  enum : Vmid
  {
    Invalid_vmid = 0,
  };

  class Space_id
  {
  public:
    Space_id()
    {
      for (unsigned i = 0; i < Max_iommus; i++)
        vmid(i, Invalid_vmid);
    }

    Space_id(Space_id const &) = delete;
    Space_id& operator = (Space_id const &) = delete;

  private:
    friend class Iommu;

    Vmid vmid(unsigned iommu_idx) const
    {
      assert(iommu_idx < Max_iommus);
      return atomic_load(&_vmids[iommu_idx]);
    }

    void vmid(unsigned iommu_idx, Vmid vmid)
    {
      assert(iommu_idx < Max_iommus);
      atomic_store(&_vmids[iommu_idx], vmid);
      // Ensure the new VMID of the page table is visible on all other cores.
      // Otherwise, a TLB flush on another core could see an outdated VMID, when
      // executed shortly after we update the VMID in Iommu::un/bind(). This is
      // because  there are no memory order guarantees as tlb_invalidate_vmid()
      // is not guarded by the IOMMU lock.
      // TODO: Is this barrier adequate to prevent the above scenario?
      Mem::dsb();
    }

    // Use a separate VMID for each IOMMU, has the advantage that we only flush
    // on IOMMUs where the page table is really bound.
    Vmid _vmids[Max_iommus];
    static_assert(sizeof(Iommu::Vmid) >= sizeof(Unsigned32),
                  "Memory accesses to VMIDs must be atomic, "
                  "therefore VMIDs must be at least 32-bit aligned.");
  };

  void tlb_flush() override;

  static void tlb_invalidate_space(Space_id const &space_id);

private:
  enum Flags
  {
    /// Stream Mapping Group modes.
    S2CR_TYPE_TRANSLATE    = 0,
    S2CR_TYPE_BYPASS       = 1,

    /// Lookup start level.
    TCR_SL0_LVL1_START     = 1,
    /// Lookup start level (Va64).
    TCR_64_SL0_LVL1_START  = 1,
    /// Outer Shareable
    TCR_SH_OS              = 2,
    /// Inner Shareable
    TCR_SH_IS              = 3,
    /// Non-cacheable Normal memory
    TCR_RGN_NC             = 0,
    /// Write-Back, Write-Allocate cacheable
    TCR_RGN_WBWA           = 1,

    /// Translation context type.
    CBAR_TYPE_STAGE_2      = 0,

    /// Virtual address width.
    CBA2R_VA64             = 1,
  };

  /// SMMU Register Spaces.
  enum class Rs
  {
    Gr0 = 0,
    Gr1 = 1,
    Cb  = 2,
  };

  Mmio_register_block &mmio_for_reg_space(Rs rs)
  { return rs == Rs::Gr0 ? _gr0 : _gr1; }

  struct Cr0 : public Smmu_reg<Cr0, Rs::Gr0, 0x000>
  {
    /// Disable SMMU.
    CXX_BITFIELD_MEMBER(0, 0, clientpd, raw);
    /// Enable global fault reporting.
    CXX_BITFIELD_MEMBER(1, 1, gfre, raw);
    /// Enable global fault interrupts.
    CXX_BITFIELD_MEMBER(2, 2, gfie, raw);
    /// Enable global config fault reporting.
    CXX_BITFIELD_MEMBER(4, 4, gcfgfre, raw);
    /// Enable global config fault interrupts.
    CXX_BITFIELD_MEMBER(5, 5, gcfgfie, raw);
    /// Stall Disable.
    CXX_BITFIELD_MEMBER(8, 8, stalld, raw);
    /**
     * Enable fault for transactions that do not match any Stream Mapping Group,
     * i.e. disable translation bypass.
     */
    CXX_BITFIELD_MEMBER(10, 10, usfcfg, raw);
    /**
     * Enable fault for transactions that match multiple Stream Mapping Groups,
     * i.e. disable translation bypass.
     */
    CXX_BITFIELD_MEMBER(21, 21, smcfcfg, raw);
    /// Enable support for 16-bit VMIDs.
    CXX_BITFIELD_MEMBER(31, 31, vmid16en, raw);
  };

  struct Idr0 : public Smmu_reg_ro<Idr0, Rs::Gr0, 0x020>
  {
    /// Number of Stream Mapping Groups.
    CXX_BITFIELD_MEMBER_RO(0, 7, numsmrg, raw);
    /// Number of Stream ID bits.
    CXX_BITFIELD_MEMBER_RO(9, 12, numsidb, raw);
    /// Coherent Translation Table Walk.
    CXX_BITFIELD_MEMBER_RO(14, 14, cttw, raw);
    /// Number of implemented context fault interrupts.
    CXX_BITFIELD_MEMBER_RO(16, 23, numirpt, raw);
    /// Stage 2 Translation Support.
    CXX_BITFIELD_MEMBER_RO(29, 29, s2ts, raw);
  };
  struct Idr1 : public Smmu_reg_ro<Idr1, Rs::Gr0, 0x024>
  {
    /// Number of Context Banks.
    CXX_BITFIELD_MEMBER_RO(0, 7, numcb, raw);
    /// Number of page index bits. #pages = 2^(#page_index_bits+1)
    CXX_BITFIELD_MEMBER_RO(28, 30, numpagendxb, raw);
    /// SMMU page size: 0 -> 4KB, 1 -> 64KB.
    CXX_BITFIELD_MEMBER_RO(31, 31, pagesize, raw);
  };
  struct Idr2 : public Smmu_reg_ro<Idr2, Rs::Gr0, 0x028>
  {
    /// Input address size.
    CXX_BITFIELD_MEMBER_RO(0, 3, ias, raw);
    /// Output address size.
    CXX_BITFIELD_MEMBER_RO(4, 7, oas, raw);
  };

  struct Gfar : public Smmu_reg<Gfar, Rs::Gr0, 0x040, Unsigned64> {};
  struct Gfsr : public Smmu_reg<Gfsr, Rs::Gr0, 0x048>
  {
    // Unidentified stream fault.
    CXX_BITFIELD_MEMBER_RO(1, 1, usf, raw);
  };
  struct Gfsynr0 : public Smmu_reg<Gfsynr0, Rs::Gr0, 0x050> {};
  struct Gfsynr1 : public Smmu_reg<Gfsynr1, Rs::Gr0, 0x054>
  {
    // StreamID of the transaction that caused the fault.
    CXX_BITFIELD_MEMBER_RO(0, 15, sid, raw);
  };
  struct Gfsynr2 : public Smmu_reg<Gfsynr2, Rs::Gr0, 0x058> {};

  struct Tlbivmid    : public Smmu_reg<Tlbivmid, Rs::Gr0, 0x064> {};
  struct Tlbiallnsnh : public Smmu_reg<Tlbiallnsnh, Rs::Gr0, 0x068> {};
  struct Tlbgsync    : public Smmu_reg<Tlbgsync, Rs::Gr0, 0x070> {};
  struct Tlbgstatus  : public Smmu_reg_ro<Tlbgstatus, Rs::Gr0, 0x074>
  {
    /// A global TLB synchronization operation is active.
    CXX_BITFIELD_MEMBER_RO(0, 0, gsactive, raw);
  };

  struct Smr : public Smmu_reg<Smr, Rs::Gr0, 0x800>
  {
    /// The Stream Identifier to match.
    CXX_BITFIELD_MEMBER(0, 14, id, raw);
    /// Mask to apply to the stream ID before comparison.
    CXX_BITFIELD_MEMBER(16, 30, mask, raw);
    /// Mark entry as valid.
    CXX_BITFIELD_MEMBER(31, 31, valid, raw);
  };

  struct S2cr : public Smmu_reg<S2cr, Rs::Gr0, 0xc00>
  {
    /// Context Bank Index.
    CXX_BITFIELD_MEMBER(0, 7, cbndx, raw);
    /// Mode of this Stream Mapping Group.
    CXX_BITFIELD_MEMBER(16, 17, type, raw);
  };

  struct Cbar : public Smmu_reg<Cbar, Rs::Gr1, 0x000>
  {
    /// Virtual Machine Identifier.
    CXX_BITFIELD_MEMBER(0, 7, vmid, raw);
    /// Translation context type.
    CXX_BITFIELD_MEMBER(16, 17, type, raw);
    /// Interrupt Index.
    CXX_BITFIELD_MEMBER(24, 31, irptndx, raw);
  };

  struct Cbfrsynra : public Smmu_reg<Cbfrsynra, Rs::Gr1, 0x400> {};

  struct Cba2r : public Smmu_reg<Cba2r, Rs::Gr1, 0x800>
  {
    /// Virtual address width.
    CXX_BITFIELD_MEMBER(0, 0, va64, raw);
  };

  struct Cb_sctlr : public Smmu_reg<Cb_sctlr, Rs::Cb, 0x000>
  {
    /// Context Bank is enabled.
    CXX_BITFIELD_MEMBER(0, 0, m, raw);
    /// Enable fault reporting for this Context Bank.
    CXX_BITFIELD_MEMBER(5, 5, cfre, raw);
    /// Enable fault interrupts for this Context Bank.
    CXX_BITFIELD_MEMBER(6, 6, cfie, raw);
    /// Hit Under Previous Context Fault.
    CXX_BITFIELD_MEMBER(8, 8, hupcf, raw);
  };

  struct Cb_ttbr0 : public Smmu_reg<Cb_ttbr0, Rs::Cb, 0x020, Unsigned64> {};

  struct Cb_tcr : public Smmu_reg<Cb_tcr, Rs::Cb, 0x030>
  {
    /**
     * Size offset as 4-bit signed number.
     * Va64 disabled: Region size is 2^(32 - T0SZ).
     * Va64 enabled: Region size is 2^(64 - T0SZ).
     */
    CXX_BITFIELD_MEMBER(0, Iommu::Va64_support ? 5 : 3, t0sz, raw);
    /// Lookup start level.
    CXX_BITFIELD_MEMBER(6, 7, sl0, raw);
    /// Inner cacheability attributes for page table memory.
    CXX_BITFIELD_MEMBER(8, 9, irgn0, raw);
    /// Outer cacheability attributes for page table memory.
    CXX_BITFIELD_MEMBER(10, 11, orgn0, raw);
    /// Shareability attributes for page table memory.
    CXX_BITFIELD_MEMBER(12, 13, sh0, raw);

    /// Physical address size (only Va64).
    CXX_BITFIELD_MEMBER(16, 18, pasize, raw);
  };

  struct Cb_fsr    : public Smmu_reg<Cb_fsr, Rs::Cb, 0x058> {};
  struct Cb_far    : public Smmu_reg<Cb_far, Rs::Cb, 0x060, Unsigned64> {};
  struct Cb_fsynr0 : public Smmu_reg<Cb_fsynr0, Rs::Cb, 0x068> {};
  struct Cb_fsynr1 : public Smmu_reg<Cb_fsynr1, Rs::Cb, 0x06c> {};

  Unsigned32 num_of_stream_ids() const
  { return 1 << _num_stream_id_bits; }

  class Context_bank
  {
  private:
    Iommu *_mmu;
    Unsigned64 _pt_phys = 0;
    Space_id *_space_id = nullptr;
    Mmio_register_block _regs;
    Unsigned32 _count = 0;
    Unsigned8 _idx;
    Unsigned8 _irptndx = 0;

    template<typename REG, Reg_access ACCESS = Reg_access::Atomic>
    REG read_reg()
    {
      if (REG::reg_space() == Rs::Cb)
        return REG::template read<ACCESS>(_regs);
      else
        return _mmu->template read_reg<REG, ACCESS>(_idx);
    }

    template<typename REG, Reg_access ACCESS = Reg_access::Atomic>
    void write_reg(REG reg)
    {
      if (REG::reg_space() == Rs::Cb)
        reg.template write<ACCESS>(_regs);
      else
        _mmu->template write_reg<REG, ACCESS>(reg, _idx);
    }

    template<typename REG, Reg_access ACCESS = Reg_access::Atomic>
    void write_reg(typename REG::Val_type value)
    { return write_reg<REG, ACCESS>(REG::from_raw(value)); }

  public:
    bool is_used() const
    { return _pt_phys != 0; }

    void inc_ref()
    { _count++; }

    Unsigned32 dec_ref()
    { return --_count; }

    Unsigned8 idx() const
    { return _idx; }

    Vmid vmid() const
    {
      // Use the context bank index + 1 as VMID, because we use VMID 0 to signal
      // an invalid VMID.
      return _idx + 1;
    }

    void irptndx(Unsigned8 irptndx)
    {
      _irptndx = irptndx;
    }

    Unsigned64 pt_phys() const
    { return _pt_phys; }

    void reset()
    {
      _pt_phys = 0;
      // Page table is no longer bound to the context bank, invalidate VMID of
      // Dmar_space.
      if (_space_id)
        {
          _space_id->vmid(_mmu->idx(), Invalid_vmid);
          _space_id = nullptr;
        }

      // Disable context bank
      write_reg<Cb_sctlr>(0);

      // TLBs have to be invalidated after context bank is disabled
      _mmu->tlb_invalidate_vmid(vmid());

      if (Iommu::Log_faults)
        {
          // Clear context fault related registers
          write_reg<Cb_fsr>(~0U);
          write_reg<Cb_far, Reg_access::Non_atomic>(0ULL);
          write_reg<Cb_fsynr0>(0);
          write_reg<Cb_fsynr1>(0);
          write_reg<Cbfrsynra>(0);
        }
    }

    void init(Iommu *mmu, Unsigned8 idx, void *reg_addr)
    {
      _mmu = mmu;
      _idx = idx;
      _regs = Mmio_register_block(reg_addr);
      reset();
    }

    void set(Address pt_phys, Space_id *space_id)
    {
      // If context bank is already in use, just increase the reference count.
      if (is_used())
        {
          // Identical page tables are managed by the same context bank.
          assert(_pt_phys == pt_phys);
          return;
        }

      _pt_phys = pt_phys;
      _space_id = space_id;

      // Set VMID of the bound space for this IOMMU. Must happen before configuring
      // the Dmar_space on the IOMMU, to ensure that TLB flushes from other cores
      // see the correct VMID!
      if (_space_id)
        _space_id->vmid(_mmu->idx(), vmid());

      // TLBs have to be invalidated before context bank is enabled
      _mmu->tlb_invalidate_vmid(vmid());

      // Configure VMID and context bank type.
      Cbar cbar;
      cbar.vmid() = vmid();
      cbar.type() = CBAR_TYPE_STAGE_2;
      if (Iommu::Log_faults)
        cbar.irptndx() = _irptndx;
      write_reg(cbar);

      Cb_tcr tcr;
      // Configure shareability and cacheability for memory associated with
      // the IOMMU page tables, according to the IOMMU's support for coherent
      // page table walks.
      if (Iommu::Coherent)
        {
          tcr.sh0() = TCR_SH_IS;
          tcr.irgn0() = TCR_RGN_WBWA;
          tcr.orgn0() = TCR_RGN_WBWA;
        }
      else
        {
          tcr.sh0() = TCR_SH_OS;
          tcr.irgn0() = TCR_RGN_NC;
          tcr.orgn0() = TCR_RGN_NC;
        }

      if (Iommu::Va64_support)
        {
          tcr.pasize() = address_size_encode(_mmu->_oas);
          // First page table is concatenated (10-bits), we skip level zero.
          tcr.sl0() = TCR_64_SL0_LVL1_START;
          // Region size is 2^(64 - T0SZ) -> T0SZ = 64 - input_address_size
          tcr.t0sz() = 64 - _mmu->_ias;

          // Enable Aarch64 translation scheme
          Cba2r cba2r;
          cba2r.va64() = CBA2R_VA64;
          write_reg(cba2r);
        }
      else
        {
          // We start at first level (Aarch32 page tables have no level zero).
          tcr.sl0() = TCR_SL0_LVL1_START;
          // Region size is 2^(32 - T0SZ) -> T0SZ = 32 - input_address_size
          tcr.t0sz() = 32 - _mmu->_ias;
        }

      write_reg(tcr);
      write_reg<Cb_ttbr0, Reg_access::Non_atomic>(pt_phys);

      // Enable context bank
      Cb_sctlr sctlr;
      sctlr.m() = 1;
      // Return an abort to the device if a context fault occurs.
      sctlr.cfre() = 1;
      sctlr.cfie() = Iommu::Log_faults;
      // Process transactions independently of any outstanding context fault.
      sctlr.hupcf() = 1;
      write_reg(sctlr);
    }

    void handle_fault();
  };

  class Stream_mapping
  {
  private:
    Iommu *_mmu;
    Context_bank *_cb = nullptr;
    Unsigned32 _idx;
    Unsigned16 _sid;
    unsigned _mask;

    template<typename REG>
    void write_reg(REG reg) const
    { _mmu->write_reg(reg, _idx); }

  public:
    bool is_used() const
    { return _cb != nullptr; }

    /// Only valid if `is_used() == true`.
    Unsigned16 sid() const
    { return _sid; }

    void init(Iommu *mmu, Unsigned32 idx, unsigned mask)
    {
      _mmu = mmu;
      _idx = idx;
      _mask = mask;
      reset();
    }

    void reset()
    {
      Smr smr;
      smr.valid() = 0;
      write_reg(smr);

      S2cr s2cr;
      s2cr.type() = S2CR_TYPE_BYPASS;
      write_reg(s2cr);

      reset_cb();
    }

    void reset_if(Context_bank *cb)
    {
      if (_cb == cb)
        reset();
    }

    void reset_cb()
    {
      if (!_cb)
        return;

      // Reset old context bank if no longer referenced.
      if (_cb->dec_ref() == 0)
        _cb->reset();

      _cb = nullptr;
    }

    void set(Context_bank *cb, Unsigned16 sid)
    {
      assert(cb != nullptr);

      // Setup the same group again, must be the same sid too, as _cb and _sid
      // are reset at the same time in reset_cb(), and a context bank is only
      // reset if no stream mapping refers to it anymore.
      if (_cb == cb)
        {
          assert(_sid == sid);
          return;
        }

      reset_cb();

      _sid = sid;
      _cb = cb;
      _cb->inc_ref();

      // Configure stream ID to context bank mapping.
      S2cr s2cr;
      s2cr.cbndx() = _cb->idx();
      s2cr.type() = S2CR_TYPE_TRANSLATE;
      write_reg(s2cr);
      /*
       * SMR 30:16 is a mask with the following semantics:
       * mask[i] == 1: sid[i] is ignored
       * mask[i] == 0: sid[i] relevant for matching
       * As we do not support any sophisticated matching mechanisms, we just set
       * the valid number of stream ID bits read from IDR0 to 0.
       */
      Smr smr;
      smr.id() = _sid;
      smr.mask() = _mask;
      smr.valid() = 1;
      write_reg(smr);
    }
  };

  Version _version;

  /// Input address size.
  Unsigned8 _ias;
  /// Output address size.
  Unsigned8 _oas;
  /// Number of implemented stream ID bits.
  Unsigned16 _num_stream_id_bits;

  Spin_lock<> _lock;

  /// Global Register Base 0.
  Mmio_register_block _gr0;
  /// Global Register Base 1.
  Mmio_register_block _gr1;

  using Cb_vect = cxx::static_vector<Context_bank>;
  Cb_vect _cb;

  using Sm_vect = cxx::static_vector<Stream_mapping>;
  Sm_vect _sm;

  /**
   * Find Context Bank for the given page table. If a Context Bank is already
   * set up to use the same page table, reuse that Context Bank. If not, use the
   * first free Context Bank.
   */
  Context_bank *find_context_bank(Address pt_phys, bool alloc = false)
  {
    Context_bank *res = nullptr;
    for (auto &cb: _cb)
      if (alloc && !res && !cb.is_used())
        res = &cb;
      else if (cb.pt_phys() == pt_phys)
        return &cb;

    return res;
  }

  /**
   * Find Stream Mapping Group for the given stream ID. If a Stream Mapping
   * Group is already set up for the same stream ID, use that Stream Mapping
   * Group. If not, use the first free Stream Mapping Group.
   */
  Stream_mapping *find_stream_mapping_group(Unsigned16 sid, bool alloc = false)
  {
    Stream_mapping *res = nullptr;
    for (auto &s: _sm)
      if (!s.is_used())
        {
          if (alloc && !res)
            res = &s;
        }
      else if (s.sid() == sid)
        return &s;

    return res;
  }

  void global_tlb_sync()
  {
    write_reg<Tlbgsync>(0);

    // XXX do not loop foreveer
    while (read_reg<Tlbgstatus>().gsactive())
      ;
  }

  static constexpr Unsigned8 address_size(Unsigned8 encoded)
  {
    constexpr Unsigned8 const sizes[] = {32, 36, 40, 42, 44, 48};
    return encoded < cxx::size(sizes) ? sizes[encoded] : 0;
  }

  static constexpr Unsigned8 address_size_encode(Unsigned8 size)
  {
    switch (size)
      {
        case address_size(0): return 0;
        case address_size(1): return 1;
        case address_size(2): return 2;
        case address_size(3): return 3;
        case address_size(4): return 4;
        case address_size(5): return 5;
        default: return 5;
      }
  }

  /**
   * Configures and enables the IOMMU.
   *
   * The mask allows to remove common unrelated parts when comparing the stream
   * id's. This can be for example the encoded TBU's (Translation Buffer Units).
   */
  void setup(Version version, void *base_addr, unsigned mask = ~0U);

  /**
   * Allocates and configures fault reporting interrupts for the IOMMU.
   *
   * \pre setup() must have been invoked
   */
  void setup_irqs(unsigned const *irqs, unsigned num_irqs, unsigned num_global_irqs);

public:
  Iommu() = default;
};

// ------------------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "panic.h"

PUBLIC
int
Iommu::bind(Unsigned32 stream_id, Address pt_phys, Space_id *space_id)
{
  auto g = lock_guard(_lock);

  if (stream_id >= num_of_stream_ids())
    return -L4_err::ERange;

  Unsigned16 sid = stream_id;
  auto *cb = find_context_bank(pt_phys, true);
  if (!cb)
    return -L4_err::ENomem;

  auto *smg = find_stream_mapping_group(sid, true);
  if (!smg)
    return -L4_err::ENomem;

  cb->set(pt_phys, space_id);
  smg->set(cb, sid);

  return 0;
}

PUBLIC
int
Iommu::unbind(Unsigned32 stream_id, Address pt_phys)
{
  auto g = lock_guard(_lock);

  if (stream_id >= num_of_stream_ids())
    return -L4_err::ERange;

  Unsigned16 sid = stream_id;
  auto *cb = find_context_bank(pt_phys, false);
  // If pt_phys is not mapped in any context bank, ignore.
  if (!cb)
    return 0;

  auto *smg = find_stream_mapping_group(sid, false);
  // If SID is not in use, ignore.
  if (!smg)
    return 0;

  // Only reset stream mapping group if it is bound to the context bank for
  // that pt_phys is bound.
  smg->reset_if(cb);

  return 0;
}

PUBLIC
void
Iommu::remove(Address pt_phys)
{
  auto g = lock_guard(_lock);

  auto *cb = find_context_bank(pt_phys, false);
  if (!cb)
    return;

  for (auto &smg: _sm)
      smg.reset_if(cb);
}

IMPLEMENT
void
Iommu::setup(Version version, void *base_addr, unsigned mask)
{
  _version = version;
  _gr0 = Mmio_register_block(base_addr);

  // Obtain hardware configuration from identification registers.
  Idr0 idr0 = read_reg<Idr0>();
  Idr1 idr1 = read_reg<Idr1>();
  Idr2 idr2 = read_reg<Idr2>();

  if (!idr0.s2ts())
    panic("IOMMU: SMMU does not support stage 2 translation.");

  if (idr0.cttw() != Iommu::Coherent)
    WARN("IOMMU: Configured as %sdma-coherent, "
         "but SMMU reports that coherent page table walks are %ssupported.\n",
         Iommu::Coherent ? "" : "non-", idr0.cttw() ? "" : "un");

  _ias = address_size(idr2.ias());
  if (_ias < Virt_addr_size)
    panic("IOMMU: Supported input address size smaller than expected: %u\n",
          _ias);
  else
    _ias = Virt_addr_size;

  _oas = address_size(idr2.oas());
  if (_oas < Phys_addr_size)
    WARN("IOMMU: Supported physical address size smaller than expected: %u\n",
         _oas);
  else
    _oas = Phys_addr_size;

  unsigned num_stream_mapping_groups = idr0.numsmrg();
  _num_stream_id_bits = idr0.numsidb();
  unsigned num_context_banks = idr1.numcb();

  if (Iommu::Debug)
      printf("IOMMU: SMMUv%d idx=%u ias=%u oas=%u va64=%d #cb=%u #smg=%u\n",
             _version == Version::Smmu_v1 ? 1 : 2, idx(), _ias, _oas,
             Va64_support, num_context_banks, num_stream_mapping_groups);

  // NUMPAGE = 2 ^ (NUMPAGENDXB + 1)
  unsigned num_pages = 1 << (idr1.numpagendxb() + 1);
  // SMMU register space page size is either 4KB or 64KB.
  unsigned pageshift = idr1.pagesize() ? 16 : 12;
  unsigned pagesize = 1 << pageshift;

  _gr1 = Mmio_register_block(offset_cast<void *>(base_addr, pagesize));

  _cb = Cb_vect(new Boot_object<Context_bank>[num_context_banks],
                num_context_banks);

  for (unsigned i = 0; i < num_context_banks; ++i)
    _cb[i].init(this, i,
                offset_cast<void *>(base_addr, (num_pages + i) * pagesize));

  _sm = Sm_vect(new Boot_object<Stream_mapping>[num_stream_mapping_groups],
                num_stream_mapping_groups);

  if (mask == ~0U)
    // Auto set mask if not provided
    mask = ~0U << _num_stream_id_bits;

  for (unsigned i = 0; i < num_stream_mapping_groups; ++i)
    _sm[i].init(this, i, mask);

  // Clear GFSR
  write_reg<Gfsr>(~0U);

  // Setup CR0
  Cr0 cr0;
  cr0.clientpd() = 0; // Enable SMMU
  cr0.gfre() = Iommu::Log_faults; // Enable fault reporting
  cr0.gfie() = Iommu::Log_faults; // Enable fault interrupts
  cr0.gcfgfre() = Iommu::Log_faults; // Enable config fault reporting
  cr0.gcfgfie() = Iommu::Log_faults; // Enable config fault interrupts
  cr0.stalld() = 1; // Disable stalling on context faults.
  if constexpr (TAG_ENABLED(iommu_passthrough))
    cr0.usfcfg() = 0; // Bypass for unknown stream IDs
  else
    cr0.usfcfg() = 1; // Faults for unknown stream IDs
  cr0.smcfcfg() = 1; // Faults on multiple matches
  cr0.vmid16en() = 0; // 8-bit VMIDs
  write_reg(cr0);

  register_iommu_tlb();
}

IMPLEMENT
void
Iommu::tlb_flush()
{
  tlb_invalidate_all();
}

PRIVATE static
void
Iommu::sync_pte()
{
  // Ensure that PTE writes are visible to IOMMU. Only necessary if IOMMU
  // supports coherent page table walks, otherwise changed PTE entries are
  // already cleaned from the dcache in Dmar_space immediately after they are
  // written.
  if (Iommu::Coherent)
    Mem::wmb();
}

PRIVATE
void
Iommu::tlb_invalidate_all()
{
  sync_pte();
  // Invalidate all non-secure non-hyp TLB entries, which is equivalent to
  // invalidating all TLB entries, since we only use stage 2 translation
  // contexts.
  write_reg<Tlbiallnsnh>(0);
  global_tlb_sync();
}

PRIVATE
void
Iommu::tlb_invalidate_vmid(Vmid vmid)
{
  // When Dmar_space is not bound to a context bank, it has the invalid VMID.
  if (vmid != Invalid_vmid)
    {
      sync_pte();
      write_reg<Tlbivmid>(vmid);
      global_tlb_sync();
    }
}

IMPLEMENT
void
Iommu::tlb_invalidate_space(Space_id const &space_id)
{
  for (unsigned i = 0; i < Max_iommus; i++)
    iommus()[i].tlb_invalidate_vmid(space_id.vmid(i));
}

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && !debug]:

#include "irq_mgr.h"

EXTENSION class Iommu
{
public:
  enum
  {
    Debug      = 0,
    Log_faults = 0,
  };
};


IMPLEMENT
void
Iommu::setup_irqs(unsigned const *, unsigned, unsigned)
{
}

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && debug]:

#include "config.h"
#include "irq_mgr.h"
#include "warn.h"
#include <cstdio>

EXTENSION class Iommu
{
public:
  enum
  {
    Debug      = 1,
    Log_faults = Warn::is_enabled(Info),
  };
};

namespace {

template<typename T, typename F>
class Smmu_irq : public Irq_base
{
public:
  explicit Smmu_irq(T *obj, F func) : _obj(obj), _func(func)
  { set_hit(handler_wrapper<Smmu_irq>); }

  void FIASCO_FLATTEN handle(Upstream_irq const *ui)
  {
    (_obj->*_func)();
    chip()->ack(pin());
    Upstream_irq::ack(ui);
  }

private:
  void switch_mode(bool) override {}
  T *_obj;
  F _func;
};

template<typename T, typename F>
void setup_irq(unsigned pin, T *obj, F func)
{
  auto irq = new Boot_object<Smmu_irq<T, F>>(obj, func);
  check(Irq_mgr::mgr->gsi_attach(irq, pin));
  irq->unmask();
}

}

PRIVATE
void
Iommu::handle_global_fault()
{
  Gfsr gfsr = read_reg<Gfsr>();
  if (!gfsr.raw)
    return;

  Gfar gfar = read_reg<Gfar, Reg_access::Non_atomic>();
  Gfsynr0 gfsynr0 = read_reg<Gfsynr0>();
  Gfsynr1 gfsynr1 = read_reg<Gfsynr1>();
  Gfsynr2 gfsynr2 = read_reg<Gfsynr2>();

  // Clear global faults
  write_reg(gfsr);

  if (gfsr.usf())
    printf("IOMMU: Blocked unknown stream ID %u!\n", gfsynr1.sid().get());
  else
    printf("IOMMU: Unexpected global fault!\n");

  printf(
    "GFSR=0x%08x GFAR=0x%016llx GFSYNR0=0x%08x GFSYNR1=0x%08x GFSYNR2=0x%08x\n",
    gfsr.raw, gfar.raw, gfsynr0.raw, gfsynr1.raw, gfsynr2.raw);
}

IMPLEMENT
void
Iommu::Context_bank::handle_fault()
{
  Cb_fsr fsr = read_reg<Cb_fsr>();
  if (!fsr.raw)
    return;

  Cb_far far = read_reg<Cb_far, Reg_access::Non_atomic>();
  Cb_fsynr0 fsynr0 = read_reg<Cb_fsynr0>();
  Cb_fsynr1 fsynr1 = read_reg<Cb_fsynr1>();
  Cbfrsynra frsynra = read_reg<Cbfrsynra>();

  // Clear context faults
  write_reg(fsr);

  printf("IOMMU: Unhandled context fault for context bank %u!\n", _idx);
  printf(
    "FSR=0x%08x FAR=0x%016llx FSYNR0=0x%08x FSYNR1=0x%08x CBFRSYNRA=0x%08x\n",
    fsr.raw, far.raw, fsynr0.raw, fsynr1.raw, frsynra.raw);
}

PRIVATE
void
Iommu::handle_context_fault()
{
  for (auto &cb : _cb)
    cb.handle_fault();
}

PRIVATE
void
Iommu::handle_fault()
{
  handle_global_fault();
  handle_context_fault();
}

IMPLEMENT
void
Iommu::setup_irqs(unsigned const *irqs, unsigned num_irqs, unsigned num_global_irqs)
{
  assert(num_irqs >= 1);
  assert(num_global_irqs <= num_irqs);

  // IRQs are only used if logging of faults is enabled.
  if (!Iommu::Log_faults)
    return;

  // In case the platform exposes only a single SMMU IRQ, but the SMMU reports
  // that it implements context interrupts, we assume that the IRQ signals
  // global faults as well as context faults.
  if (num_irqs == 1 && read_reg<Idr0>().numirpt() != 0)
    {
      setup_irq(irqs[0], this, &Iommu::handle_fault);
      return;
    }

  // Setup global fault IRQs
  for (unsigned i = 0; i < num_global_irqs; i++)
    setup_irq(irqs[i], this, &Iommu::handle_global_fault);

  // Setup context fault IRQs
  unsigned num_context_irqs = num_irqs - num_global_irqs;
  if (num_context_irqs == 0)
    // SMMU does not implement context fault IRQs.
    return;

  if (num_context_irqs >= _cb.size())
    {
      // Each context bank gets its own IRQ.
      for (unsigned i = 0; i < _cb.size(); ++i)
        {
          setup_irq(irqs[num_global_irqs + i], &_cb[i],
                    &Context_bank::handle_fault);
          _cb[i].irptndx(i);
        }
    }
  else
    {
      if (_version == Version::Smmu_v1)
        {
          // Not enough context IRQs, all context banks get the same IRQ.
          setup_irq(irqs[num_global_irqs], this, &Iommu::handle_context_fault);
        }
      else
        {
          WARN("SMMUv2 is expected to have a dedicated IRQ per context bank, "
               "but instead of %u only %u context interrupts were provided.\n",
               _cb.size(), num_context_irqs);
        }
    }

}
