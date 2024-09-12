INTERFACE [iommu && iommu_arm_smmu_v3]:

#include "types.h"
#include "spin_lock.h"
#include "boot_alloc.h"
#include "mem_chunk.h"
#include "minmax.h"
#include "mmio_register_block.h"
#include "panic.h"
#include "cxx/bitfield"
#include "kmem_slab.h"
#include "simple_id_alloc.h"
#include "timer.h"

class Iommu_domain;

/*
 * Iommu implementation for ARM SMMUv3.
 *
 * The mapping between a device and a page table works as follows: Each device
 * is identified by a `stream ID`. The SMMU uses the stream ID as an index into
 * the stream table, thereby locating a corresponding stream table entry.
 * The stream table entry, in case of stage 1 translation with a context
 * descriptor as further indirection, contains the configuration and address of
 * the assigned page table. Part of this configuration is an ASID to allow for
 * efficient TLB maintenance.
 *
 * When the ARM_IOMMU_STAGE2 Kconfig option is set, the SMMU is configured to
 * use stage 2 translation, otherwise stage 1 translation is used.
 *
 * The SMMU is configured, so that devices for whose stream ID no mapping
 * exists are blocked by the SMMU, and therefore cannot access physical memory
 * via DMA.
 *
 * Interrupts are used to handle errors, and to log faults if the warning level
 * is high enough.
 */
EXTENSION class Iommu
{
public:
  enum
  {
    First_asid   = 0,
    Max_nr_asid  = 0x10000,
    Invalid_asid = Max_nr_asid,
    Default_vmid = 0,
  };

  using Asid = unsigned long;
  using Asid_alloc = Simple_id_alloc<Asid, First_asid, Max_nr_asid, Invalid_asid>;

  /**
   * Invalidate the entire TLB of this Iommu.
   */
  void tlb_flush() override;

  /**
   * Invalidate TLB for the given domain on all Iommus it is bound to.
   */
  static void tlb_invalidate_domain(Iommu_domain const &domain);

private:
  friend class Iommu_domain;

  enum
  {
    /// Limit size of stream table to cover at most 20 bits of stream ID,
    /// resulting in a maximum size of 128KiB for the first-level table.
    Stream_table_max_bits       = 20,
    /// 64 entries (4k page) per second-level table.
    Stream_table_split          = 6,
    Stream_table_l2_size        = 1 << Stream_table_split,
    Stream_table_l2_mask        = Stream_table_l2_size - 1,
    /// Limit number of second-level stream tables that can be allocated to
    /// avoid exhausting kernel memory.
    Stream_table_l2_alloc_limit = 64,

    /// Limit size of command queue to 256 entries (4k page), no reason to waste
    /// memory or cache here.
    Cmd_queue_max_bits          = 8,

    /// Limit size of event queue to 128 entries (4k page), no reason to waste
    /// memory or cache here.
    Event_queue_max_bits        = 7,
  };

  /// SMMU Register Spaces.
  enum class Rs
  {
    Rp0 = 0,
    Rp1 = 1,
  };

  Mmio_register_block &mmio_for_reg_space(Rs rs)
  { return rs == Rs::Rp0 ? _rp0 : _rp1; }

  struct Idr0 : public Smmu_reg_ro<Idr0, Rs::Rp0, 0x000>
  {
    enum
    {
      Ttf_aarch64         = 2,
      Ttf_aarch32_aarch64 = 3,

      Stall_model_forced = 2,
    };

    /// Stage2 translation supported
    CXX_BITFIELD_MEMBER_RO(0, 0, s2p, raw);
    /// Stage1 translation supported.
    CXX_BITFIELD_MEMBER_RO(1, 1, s1p, raw);
    /// Translation Table Formats supported at both stage 1 and stage 2.
    CXX_BITFIELD_MEMBER_RO(2, 3, ttf, raw);
    /// Coherent access supported to translations, structures and queues.
    CXX_BITFIELD_MEMBER_RO(4, 4, cohacc, raw);
    /// Broadcast TLB Maintenance
    CXX_BITFIELD_MEMBER_RO(5, 5, btm, raw);
    /// 16-bit ASID support.
    CXX_BITFIELD_MEMBER_RO(12, 12, asid16, raw);
    /// Message Signalled Interrupts are supported.
    CXX_BITFIELD_MEMBER_RO(13, 13, msi, raw);
    /// SMMU, and system, support generation of WFE wake-up events to PE.
    CXX_BITFIELD_MEMBER_RO(14, 14, sev, raw);
    /// 16-bit VMID support.
    CXX_BITFIELD_MEMBER_RO(18, 18, vmid16, raw);
    /// Stall model support.
    CXX_BITFIELD_MEMBER_RO(24, 25, stall_model, raw);
    /// Multi-level Stream table support.
    CXX_BITFIELD_MEMBER_RO(27, 28, st_level, raw);
  };

  struct Idr1 : public Smmu_reg_ro<Idr1, Rs::Rp0, 0x004>
  {
    /// Max bits of StreamID.
    CXX_BITFIELD_MEMBER_RO(0, 5, sidsize, raw);
    /// Max bits of SubstreamID.
    CXX_BITFIELD_MEMBER_RO(6, 10, ssidsize, raw);
    /// Maximum number of PRI queue entries.
    CXX_BITFIELD_MEMBER_RO(11, 15, priqs, raw);
    /// Maximum number of Event queue entries.
    CXX_BITFIELD_MEMBER_RO(16, 20, eventqs, raw);
    /// Maximum number of Command queue entries.
    CXX_BITFIELD_MEMBER_RO(21, 25, cmdqs, raw);
    /// Relative base pointers.
    CXX_BITFIELD_MEMBER_RO(28, 28, rel, raw);
    /// Queue base addresses fixed.
    CXX_BITFIELD_MEMBER_RO(29, 29, queues_preset, raw);
    /// Table base addresses fixed.
    CXX_BITFIELD_MEMBER_RO(30, 30, tables_preset, raw);
    /// Support for enhanced Command queue interface.
    CXX_BITFIELD_MEMBER_RO(31, 31, ecmdq, raw);
  };

  struct Idr5 : public Smmu_reg_ro<Idr5, Rs::Rp0, 0x014>
  {
    /// Output Address Size.
    CXX_BITFIELD_MEMBER_RO( 0,  2, oas, raw);
    /// 4KB translation granule supported.
    CXX_BITFIELD_MEMBER_RO( 4,  4, gran4k, raw);
    /// Virtual Address eXtend.
    CXX_BITFIELD_MEMBER_RO(10, 11, vax, raw);
  };

  struct Cr0 : public Smmu_reg<Cr0, Rs::Rp0, 0x020>
  {
    /// Non-secure SMMU enable.
    CXX_BITFIELD_MEMBER(0, 0, smmuen, raw);
    /// Enable PRI queue writes.
    CXX_BITFIELD_MEMBER(1, 1, priqen, raw);
    /// Enable Event queue writes.
    CXX_BITFIELD_MEMBER(2, 2, eventqen, raw);
    /// Enable Command queue processing.
    CXX_BITFIELD_MEMBER(3, 3, cmdqen, raw);
  };

  struct Cr0ack : public Smmu_reg_ro<Cr0ack, Rs::Rp0, 0x024>
  {};

  struct Cr1 : public Smmu_reg<Cr1, Rs::Rp0, 0x028>
  {
    enum
    {
      /// Inner Shareable.
      Share_is = 3,
      /// Write-Back Cacheable.
      Cache_wb = 1,
    };

    /// Queue access Inner Cacheability.
    CXX_BITFIELD_MEMBER( 0,  1, queue_ic, raw);
    /// Queue access Outer Cacheability.
    CXX_BITFIELD_MEMBER( 2,  3, queue_oc, raw);
    /// Queue access Shareability.
    CXX_BITFIELD_MEMBER( 4,  5, queue_sh, raw);
    /// Table access Inner Cancheability.
    CXX_BITFIELD_MEMBER( 6,  7, table_ic, raw);
    /// Table access Outer Cacheability.
    CXX_BITFIELD_MEMBER( 8,  9, table_oc, raw);
    /// Table access Shareability.
    CXX_BITFIELD_MEMBER(10, 11, table_sh, raw);
  };

  struct Cr2 : public Smmu_reg<Cr2, Rs::Rp0, 0x02c>
  {
    /// Private TLB Maintenance.
    CXX_BITFIELD_MEMBER( 2,  2, ptm, raw);
  };

  struct Gbpa : public Smmu_reg<Gbpa, Rs::Rp0, 0x044>
  {
    /// Abort all incoming transactions.
    CXX_BITFIELD_MEMBER(20, 20, abort, raw);
    /// Update completion flag.
    CXX_BITFIELD_MEMBER(31, 31, update, raw);
  };

  struct Irq_ctrl : public Smmu_reg<Irq_ctrl, Rs::Rp0, 0x050>
  {
    /// GERROR interrupt enable.
    CXX_BITFIELD_MEMBER(0, 0, gerror_irqen, raw);
    /// PRI queue interrupt enable.
    CXX_BITFIELD_MEMBER(1, 1, priq_irqen, raw);
    /// Event queue interrupt enable.
    CXX_BITFIELD_MEMBER(2, 2, eventq_irqen, raw);
  };

  struct Irq_ctrlack : public Smmu_reg_ro<Irq_ctrlack, Rs::Rp0, 0x054>
  {};

  struct Gerror : public Smmu_reg_ro<Gerror, Rs::Rp0, 0x0060>
  {
    CXX_BITFIELD_MEMBER_RO(0, 0, cmdq_err, raw);
    CXX_BITFIELD_MEMBER_RO(2, 2, eventq_abt_err, raw);
    CXX_BITFIELD_MEMBER_RO(3, 3, priq_abt_err, raw);
    CXX_BITFIELD_MEMBER_RO(4, 4, msi_cmdq_abt_err, raw);
    CXX_BITFIELD_MEMBER_RO(5, 5, msi_eventq_abt_err, raw);
    CXX_BITFIELD_MEMBER_RO(6, 6, msi_priq_abt_err, raw);
    CXX_BITFIELD_MEMBER_RO(7, 7, msi_gerror_abt_err, raw);
    CXX_BITFIELD_MEMBER_RO(8, 8, sfm_err, raw);
    CXX_BITFIELD_MEMBER_RO(9, 9, cmdqp_err, raw);
  };

  struct Gerrorn : public Smmu_reg<Gerrorn, Rs::Rp0, 0x0064>
  {};

  struct Gerror_irq_cfg0 : public Smmu_reg<Gerror_irq_cfg0, Rs::Rp0, 0x0068,
                                           Unsigned64>
  {
    CXX_BITFIELD_MEMBER(2, 51, addr, raw);
  };

  struct Strtab_base : public Smmu_reg<Strtab_base, Rs::Rp0, 0x0080, Unsigned64>
  {
    /// Physical address of Stream table base.
    CXX_BITFIELD_MEMBER_UNSHIFTED( 6, 51, addr, raw);
    /// Read-Allocate hint.
    CXX_BITFIELD_MEMBER(62, 62, ra, raw);

    static unsigned align(unsigned size)
    {
      return max(size, 1U << addr_bfm_t::Lsb);
    }
  };

  struct Strtab_base_cfg : public Smmu_reg<Strtab_base_cfg, Rs::Rp0, 0x0088>
  {
    enum
    {
      Fmt_linear = 0b00,
      Fmt_2level = 0b01,
    };

    /// Table size as log2 (entries).
    CXX_BITFIELD_MEMBER( 0,  5, log2size, raw);
    /// StreamID split point for multi-level table.
    CXX_BITFIELD_MEMBER( 6, 10, split, raw);
    /// Format of Stream table.
    CXX_BITFIELD_MEMBER(16, 17, fmt, raw);
  };

  struct Cmdq_base : public Smmu_reg<Cmdq_base, Rs::Rp0, 0x0090, Unsigned64>
  {
    /// Queue size as log2 (entries).
    CXX_BITFIELD_MEMBER( 0,  4, log2size, raw);
    /// Physical address of Command queue base.
    CXX_BITFIELD_MEMBER_UNSHIFTED( 5, 51, addr, raw);
    /// Read-Allocate hint.
    CXX_BITFIELD_MEMBER(62, 62, alloc, raw);
  };

  struct Cmdq_prod : public Smmu_reg<Cmdq_prod, Rs::Rp0, 0x0098>
  {
    /// Command queue write index.
    CXX_BITFIELD_MEMBER(0, 19, wr, raw);
  };

  struct Cmdq_cons : public Smmu_reg<Cmdq_cons, Rs::Rp0, 0x009c>
  {
    enum Err
    {
      Err_none         = 0x0,
      Err_ill          = 0x1,
      Err_abt          = 0x2,
      Err_atc_inv_sync = 0x3,
    };

    /// Command queue read index.
    CXX_BITFIELD_MEMBER_RO(0, 19, rd, raw);
    /// Error reason code.
    CXX_BITFIELD_MEMBER(24, 30, err, raw);
  };

  struct Eventq_base : public Smmu_reg<Eventq_base, Rs::Rp0, 0x00A0, Unsigned64>
  {
    /// Queue size as log2 (entries).
    CXX_BITFIELD_MEMBER( 0,  4, log2size, raw);
    /// Physical address of Event queue base.
    CXX_BITFIELD_MEMBER_UNSHIFTED( 5, 51, addr, raw);
    /// Write-Allocate hint.
    CXX_BITFIELD_MEMBER(62, 62, alloc, raw);
  };

  struct Eventq_prod : public Smmu_reg<Eventq_prod, Rs::Rp1, 0x00a8>
  {
    /// Command queue write index.
    CXX_BITFIELD_MEMBER_RO(0, 19, wr, raw);
    /// Event queue overflowed flag.
    CXX_BITFIELD_MEMBER_RO(31, 31, ovflg, raw);
  };

  struct Eventq_cons : public Smmu_reg<Eventq_cons, Rs::Rp1, 0x00ac>
  {
    /// Event queue read index.
    CXX_BITFIELD_MEMBER( 0, 19, rd, raw);
    /// Overflow acknowledge flag.
    CXX_BITFIELD_MEMBER(31, 31, ovackflg, raw);
  };

  struct Eventq_irq_cfg0 : public Smmu_reg<Eventq_irq_cfg0, Rs::Rp0, 0x00b0,
                                           Unsigned64>
  {
    CXX_BITFIELD_MEMBER(2, 51, addr, raw);
  };

  /// Level 1 Stream Table Descriptor
  struct L1std
  {
    Unsigned64 raw = 0;

    // 2^n size of Level 2 array and validity of L2Ptr.
    CXX_BITFIELD_MEMBER( 0, 4, span, raw);

    /// Pointer to start of Level-2 array.
    CXX_BITFIELD_MEMBER_UNSHIFTED( 6, 51, l2_ptr, raw);

    bool is_valid() { return span() > 0; }
  };
  static_assert(sizeof(L1std) == 8,
                "Level 1 Stream Table Descriptor has unexpected size!");

  /// Stream Table Entry (STE)
  struct Ste
  {
    Unsigned64 raw[8] = { 0 };

    enum
    {
      Config_enable       = 1 << 2,
      Config_s2_translate = 1 << 1,
      Config_s1_translate = 1 << 0,
    };

    /// STE Valid
    CXX_BITFIELD_MEMBER( 0,  0, v, raw[0]);
    /// Stream configuration
    CXX_BITFIELD_MEMBER( 1,  3, config, raw[0]);
    /// Stage 1 Format
    CXX_BITFIELD_MEMBER( 4,  5, s1_fmt, raw[0]);
    /// Stage 1 Context descriptor pointer
    CXX_BITFIELD_MEMBER_UNSHIFTED( 6, 51, s1_context_ptr, raw[0]);

    /// S1ContextPtr memory Inner Region attribute
    CXX_BITFIELD_MEMBER( 2,  3, s1_cir, raw[1]);
    /// S1ContextPtr memory Outer Region attribute
    CXX_BITFIELD_MEMBER( 4,  5, s1_cor, raw[1]);
    /// S1ContextPtr memory Shareability attribute
    CXX_BITFIELD_MEMBER( 6,  7, s1_csh, raw[1]);
    /// Stage 1 Stall Disable
    CXX_BITFIELD_MEMBER(27, 27, s1_stalld, raw[1]);
    /// StreamWorld control
    CXX_BITFIELD_MEMBER(30, 31, strw, raw[1]);
    /// Shareability configuration
    CXX_BITFIELD_MEMBER(44, 45, shcfg, raw[1]);

    /// Virtual Machine Identifier
    CXX_BITFIELD_MEMBER( 0, 15, s2_vmid, raw[2]);
    /// Size of IPA input region covered by stage 2 translation table.
    CXX_BITFIELD_MEMBER(32, 37, s2_t0sz, raw[2]);
    /// Starting level of stage 2 translation table walk.
    CXX_BITFIELD_MEMBER(38, 39, s2_sl0, raw[2]);
    /// Inner region Cacheability for stage 2 translation table access.
    CXX_BITFIELD_MEMBER(40, 41, s2_ir0, raw[2]);
    /// Outer region Cacheability for stage 2 translation table access.
    CXX_BITFIELD_MEMBER(42, 43, s2_or0, raw[2]);
    /// Shareability for stage 2 translation table access
    CXX_BITFIELD_MEMBER(44, 45, s2_sh0, raw[2]);
    /// Stage 2 Translation Granule size
    CXX_BITFIELD_MEMBER(46, 47, s2_tg, raw[2]);
    /// Physical address Size
    CXX_BITFIELD_MEMBER(48, 50, s2_ps, raw[2]);
    /// Stage 2 translation table is AArch64
    CXX_BITFIELD_MEMBER(51, 51, s2_aa64, raw[2]);
    /// Stage 2 Access Flag Fault Disable
    CXX_BITFIELD_MEMBER(53, 53, s2_affd, raw[2]);
    /// Protected Table Walk
    CXX_BITFIELD_MEMBER(54, 54, s2_ptw, raw[2]);
    /// Stage 2 fault behavior - Stall
    CXX_BITFIELD_MEMBER(57, 57, s2_s, raw[2]);
    /// Stage 2 fault behavior - Record
    CXX_BITFIELD_MEMBER(58, 58, s2_r, raw[2]);

    /// Address of Translation Table base, bits [51:4].
    CXX_BITFIELD_MEMBER_UNSHIFTED(4, 51, s2_ttb, raw[3]);
  };
  static_assert(sizeof(Ste) == 64, "Stream Table Entry has unexpected size!");

  /// Context descriptor
  struct Cd
  {
    Unsigned64 raw[8] = { 0 };

    CXX_BITFIELD_MEMBER( 0,  5, t0sz, raw[0]);
    CXX_BITFIELD_MEMBER( 6,  7, tg0, raw[0]);
    CXX_BITFIELD_MEMBER( 8,  9, ir0, raw[0]);
    CXX_BITFIELD_MEMBER(10, 11, or0, raw[0]);
    CXX_BITFIELD_MEMBER(12, 13, sh0, raw[0]);
    CXX_BITFIELD_MEMBER(14, 14, epd0, raw[0]);
    CXX_BITFIELD_MEMBER(15, 15, endi, raw[0]);
    CXX_BITFIELD_MEMBER(16, 21, t1sz, raw[0]);
    CXX_BITFIELD_MEMBER(22, 23, tg1, raw[0]);
    CXX_BITFIELD_MEMBER(24, 25, ir1, raw[0]);
    CXX_BITFIELD_MEMBER(26, 27, or1, raw[0]);
    CXX_BITFIELD_MEMBER(28, 29, sh1, raw[0]);
    CXX_BITFIELD_MEMBER(30, 30, epd1, raw[0]);
    CXX_BITFIELD_MEMBER(31, 31, v, raw[0]);
    CXX_BITFIELD_MEMBER(32, 34, ips, raw[0]);
    CXX_BITFIELD_MEMBER(35, 35, affd, raw[0]);
    CXX_BITFIELD_MEMBER(36, 36, wxn, raw[0]);
    CXX_BITFIELD_MEMBER(37, 37, uwxn, raw[0]);
    CXX_BITFIELD_MEMBER(38, 38, tbi0, raw[0]);
    CXX_BITFIELD_MEMBER(39, 39, tbi1, raw[0]);
    CXX_BITFIELD_MEMBER(40, 40, pan, raw[0]);
    CXX_BITFIELD_MEMBER(41, 41, aa64, raw[0]);
    CXX_BITFIELD_MEMBER(42, 42, hd, raw[0]);
    CXX_BITFIELD_MEMBER(43, 43, ha, raw[0]);
    CXX_BITFIELD_MEMBER(44, 44, s, raw[0]);
    CXX_BITFIELD_MEMBER(45, 45, r, raw[0]);
    CXX_BITFIELD_MEMBER(46, 46, a, raw[0]);
    CXX_BITFIELD_MEMBER(47, 47, aset, raw[0]);
    CXX_BITFIELD_MEMBER(48, 63, asid, raw[0]);

    CXX_BITFIELD_MEMBER( 0,  0, nscfg0, raw[1]);
    CXX_BITFIELD_MEMBER( 1,  1, had0, raw[1]);
    CXX_BITFIELD_MEMBER( 2,  2, e0pd0, raw[1]);
    CXX_BITFIELD_MEMBER_UNSHIFTED( 4, 51, ttb0, raw[1]);

    CXX_BITFIELD_MEMBER( 0,  0, nscfg1, raw[2]);
    CXX_BITFIELD_MEMBER( 1,  1, had1, raw[2]);
    CXX_BITFIELD_MEMBER( 2,  2, e0pd1, raw[2]);
    CXX_BITFIELD_MEMBER_UNSHIFTED( 4, 51, ttb1, raw[2]);

    CXX_BITFIELD_MEMBER( 0, 31, mair0, raw[3]);
    CXX_BITFIELD_MEMBER( 32, 63, mair1, raw[3]);

    CXX_BITFIELD_MEMBER( 0, 31, amair0, raw[4]);
    CXX_BITFIELD_MEMBER( 32, 63, amair1, raw[4]);
  } __attribute__((aligned(64)));
  static_assert(sizeof(Cd) == 64, "Context Descriptor has unexpected size!");
  static_assert(alignof(Cd) == 64, "Context Descriptor has unexpected alignment!");

  struct Cmd
  {
    Unsigned64 raw[2] = { 0 };

    enum Op
    {
      Op_cfgi_ste       = 0x03,
      Op_tlbi_nh_asid   = 0x11,
      Op_tlbi_s12_vmall = 0x28,
      Op_tlbi_nsnh_all  = 0x30,
      Op_sync           = 0x46,
    };

    Cmd() = default;
    explicit Cmd(Op cmd_op) { op() = cmd_op; };

    CXX_BITFIELD_MEMBER          ( 0,  7, op, raw[0]);
    CXX_BITFIELD_MEMBER          (32, 63, stream_id, raw[0]);
    CXX_BITFIELD_MEMBER          (12, 31, substream_id, raw[0]);
    CXX_BITFIELD_MEMBER          (32, 47, vmid, raw[0]);
    CXX_BITFIELD_MEMBER          (48, 63, asid, raw[0]);
    CXX_BITFIELD_MEMBER          (12, 16, num, raw[0]);
    CXX_BITFIELD_MEMBER          (20, 24, scale, raw[0]);
    CXX_BITFIELD_MEMBER          (12, 13, cs, raw[0]);

    CXX_BITFIELD_MEMBER          ( 0,  0, leaf, raw[1]);
    CXX_BITFIELD_MEMBER          ( 0,  4, range, raw[1]);
    CXX_BITFIELD_MEMBER          ( 8,  9, ttl, raw[1]);
    CXX_BITFIELD_MEMBER          (10, 11, tg, raw[1]);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 63, address, raw[1]);

    enum Compl_signal
    {
      Compl_signal_none = 0,
      Compl_signal_irq  = 1,
      Compl_signal_wfe  = 2,
    };

    /**
     * Invalidate the Stream Table Entry with the given StreamID, including its
     * Context Descriptors.
     */
    static Cmd cfgi_ste(Unsigned32 stream_id, bool leaf)
    {
      Cmd cmd(Op_cfgi_ste);
      cmd.stream_id() = stream_id;
      cmd.leaf() = leaf;
      return cmd;
    }

     /**
      * Invalidate all stage 1 EL1 non-global entries by VMID and ASID
      * (equivalent to ASIDE1).
      */
     static Cmd tlbi_nh_asid(Unsigned16 vmid, Unsigned16 asid)
     {
       Cmd cmd(Op_tlbi_nh_asid);
       cmd.vmid() = vmid;
       cmd.asid() = asid;
       return cmd;
     }

    /**
     * Invalidate all entries at all implemented stages by VMID
     * (equivalent to VMALLS12E1).
     */
    static Cmd tlbi_s12_vmall(Unsigned16 vmid)
    {
      Cmd cmd(Op_tlbi_s12_vmall);
      cmd.vmid() = vmid;
      return cmd;
    }

    /**
     * Invalidate all non-EL2 TLB entries at all implemented stages
     * (equivalent to ALLE1).
     */
    static Cmd tlbi_nsnh_all()
    {
      Cmd cmd(Op_tlbi_nsnh_all);
      return cmd;
    }

    /**
     * Synchronize execution of preceding commands. When the completion of this
     * command is observable, all operations of the preceding commands are
     * guaranteed to be observable as well.
     */
    static Cmd sync(Compl_signal cs)
    {
      Cmd cmd(Op_sync);
      cmd.cs() = cs;
      return cmd;
    }
  };
  static_assert(sizeof(Cmd) == 16, "Cmd has unexpected size!");

  /**
   * The SMMU uses a rather complicated mechanism to track the read and write
   * index of a queue, to avoid having an unusable entry in the queue.
   * Specifically, a wrap flag, which is toggled on wrap-around, is used to
   * distinguish an empty queue from a full queue.
   */
  template<typename T, typename RBASE, typename RPROD, typename RCONS,
           typename INDEX_TYPE, bool WRITE>
  class Queue
  {
    public:
      enum { Item_size = sizeof(T) };

      void init(Iommu *iommu, unsigned num_item_bits)
      {
        _iommu = iommu;
        _num_item_bits = num_item_bits;

        unsigned mem_size = (1 << num_item_bits) * Item_size;
        // Base address of queue must be aligned to the larger of the queue size
        // in bytes or 32 bytes.
        _mem = Mem_chunk::alloc_zmem(mem_size, max(mem_size, 32u));
        if (!_mem.is_valid())
          panic("IOMMU: Failed to allocate queue memory!");
        _index = 0;

        RBASE base;
        base.log2size() = num_item_bits;
        base.addr() = _mem.phys_addr();
        base.alloc() = 1;
        _iommu->write_reg(base);

        // Set write index to zero
        _iommu->write_reg<RPROD>(0);
        // Set read index to zero
        _iommu->write_reg<RCONS>(0);
      }

      bool is_empty() const
      {
        // "The two indexes are equal and their wrap bits are equal."
        return (_index & combined_mask()) == read_their_index();
      }

      bool is_full() const
      {
        unsigned i = read_their_index();
        // "The two indexes are equal and their wrap bits are different."
        return   (i & index_mask()) == (_index & index_mask())
               && i != (_index & combined_mask());
      }

      T *slot_at(unsigned index)
      {
        return _mem.virt_ptr<T>() + (index & index_mask());
      }

    protected:
      unsigned capacity() const
      { return (1 << _num_item_bits); }

      unsigned index_mask() const
      { return (1 << _num_item_bits) - 1; }

      unsigned wrap_flag_mask() const
      { return 1 << _num_item_bits; }

      /// Mask covering index and wrap flag.
      unsigned combined_mask() const
      { return (1 << (_num_item_bits + 1)) - 1; }

      /// Return read index if WRITE, write index otherwise.
      unsigned read_their_index() const
      {
        return WRITE ? _iommu->read_reg<RCONS>().rd()
                     : _iommu->read_reg<RPROD>().wr();
      }

      T *next_slot()
      {
        if (WRITE && is_full())
          // Queue is full, nothing can be written.
          return nullptr;

        if (!WRITE && is_empty())
          // Queue is empty, nothing can be read.
          return nullptr;

        T *slot = _mem.virt_ptr<T>() + (_index & index_mask());
        // Update next index (needs to be atomic, because _index is read
        // without cmd_queue lock).
        atomic_store(&_index, _index + 1);
        return slot;
      }

    protected:
      Iommu *_iommu;
      Mem_chunk _mem;
      /// Next write index if WRITE, next read index otherwise.
      /// Unlike in the hardware register, the index stored in this field is not
      /// truncated to `_num_item_bits`, which is helpful for checking whether
      /// an item has been consumed (see `is_item_complete()`).
      INDEX_TYPE _index;
      Unsigned8 _num_item_bits;
  };

  template<typename T, typename RBASE, typename RPROD, typename RCONS>
  class Read_queue : public Queue<T, RBASE, RPROD, RCONS, unsigned, false>
  {
    public:
      template<typename OVERFLOW_CB>
      bool read(T &out, OVERFLOW_CB&& overflow_cb)
      {
        T *slot = this->next_slot();
        if (!slot)
          return false;

        out = access_once(slot);

        RPROD prod = this->_iommu->template read_reg<RPROD>();
        if (prod.ovflg() != _ovackflg)
          {
            _ovackflg = prod.ovflg();
            overflow_cb();
          }

        RCONS cons;
        cons.rd() = this->_index;
        cons.ovackflg() = _ovackflg;
        this->_iommu->write_reg(cons);
        return true;
      }

    private:
      bool _ovackflg = false;
  };

  template<typename T, typename RBASE, typename RPROD, typename RCONS>
  class Write_queue : public Queue<T, RBASE, RPROD, RCONS, Unsigned64, true>
  {
    public:
      // Use 64-bit type to track the write queue index, so that it becomes
      // effectively impossible for the index to wrap around and catch up again
      // while waiting for a command to complete.
      using Index = Unsigned64;

      /**
       * Write given item into queue.
       *
       * \param  item        The item to write
       * \param  wait_index  The wait index for the item in the queue, can be
       *                     passed to is_item_complete, only valid on success.
       *
       * \return true if the item was written to the queue, and false if the
       *         queue was full.
       */
      bool write(T const &item, Index *wait_index = nullptr)
      {
        T *slot = this->next_slot();
        if (EXPECT_FALSE(!slot))
          return false;

        *slot = item;
        // Make item observable by the SMMU.
        make_observable(slot, slot + 1);

        RPROD prod;
        prod.wr() = this->_index;
        this->_iommu->write_reg(prod);

        if (wait_index)
          *wait_index = this->_index;
        return true;
      }

      /**
       * Determines whether the reader has consumed the given queue item.
       *
       * \pre The queue did not consume more than `(2^64) - capacity()` items
       *      since writing the item to the given index.
       *
       * \param wait_index  Index of the item whose completion is to be checked.
       */
      bool is_item_complete(Index wait_index)
      {
        // A `wait_index` is always an `_index` from the past, i.e. the `_index`
        // value directly after enqueuing an item. The distance between the
        // current `_index` and `wait_index` therefore equals the number of
        // items enqueued in the meantime. If this number is greater than or
        // equal to the capacity of the queue, we know that the item has been
        // overwritten. Wrap-around is handled gracefully thanks to unsigned
        // integer.
        if ((atomic_load(&this->_index) - wait_index) >= this->capacity())
            // The item has been overwritten, which also implies that it has
            // been consumed by the reader.
            return true;

        unsigned read_index = this->read_their_index();
        unsigned wrap_flag_mask = this->wrap_flag_mask();
        unsigned index_mask = this->index_mask();
        if ((wait_index & wrap_flag_mask) == (read_index & wrap_flag_mask))
          // Reader has passed awaited item and has not wrapped-around.
          // --------------------------------
          // |  O~~~W~~~~~~~~~~~~R          |
          // -- ^ -------------- ^ ----------
          //    | Reader before  | Reader now
          return read_index >= (wait_index & this->combined_mask());
        else
          // Reader has passed awaited item and has wrapped-around.
          // --------------------------------
          // | ~~R             O~~~W~~~~~~~ | (wrap-around)
          // --- ^ ----------- ^ ------------
          //     | Reader now  | Reader before
          return (wait_index & index_mask) > (read_index & index_mask);
      }
  };

  /// Timeout after which to warn about long running command queue operation.
  static constexpr unsigned Queue_warn_timeout_us = 1'000'000; // 1s

  class Wait_warn_timeout
  {
  public:
    inline bool check_warn(Unsigned64 timeout_us)
    {
      if (wait_start == 0)
        {
          wait_start = Timer::system_clock();
          return false;
        }

      if (!warned && (Timer::system_clock() - wait_start) >= timeout_us)
        {
          warned = true;
          return true;
        }

      return false;
    }

  private:
    Unsigned64 wait_start = 0;
    bool warned = false;
  };

  static constexpr Unsigned8 address_size(Unsigned8 encoded)
  {
    constexpr Unsigned8 sizes[] = {32, 36, 40, 42, 44, 48};
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

  Unsigned32 num_of_stream_ids() const
  { return 1 << _num_stream_id_bits; }

  unsigned _idx;

  /// Intermediate address size (output of stage 1, input of stage 2).
  Unsigned8 _ias;
  /// Output address size (output of stage 2).
  Unsigned8 _oas;
  /// Number of ASID bits.
  Unsigned8 _num_asid_bits;
  /// Number of implemented stream ID bits.
  Unsigned8 _num_stream_id_bits;

  /// Coordinates concurrent operations on this SMMU (stream table management,
  /// domain configuration and partly TLB invalidation).
  Spin_lock<> _lock;
  /// SMMU supports generation of WFE wake-up events.
  bool _support_wfe;

  /// Register Page 0.
  Mmio_register_block _rp0;
  /// Register Page 1.
  Mmio_register_block _rp1;

  /// Top-level stream table.
  Mem_chunk _strtab;
  /// Whether two-level stream table is used.
  bool _strtab_2level;
  /// Track of number of allocated second-level stream tables in order to limit
  /// it to Stream_table_l2_alloc_limit.
  unsigned _strtab_l2_alloc_cnt = 0;
  /// Coordinates concurrent allocation of second-level stream tables (protects
  /// _strtab and _strtab_l2_alloc_cnt).
  Spin_lock<> _strtab_l2_alloc_lock;

  /// Command queue.
  using Cmd_queue = Write_queue<Cmd, Cmdq_base, Cmdq_prod, Cmdq_cons>;
  Cmd_queue _cmd_queue;
  /// Coordinates concurrent access to the command queue.
  Spin_lock<> _cmd_queue_lock;

  /// ASID allocator shared between all SMMUs.
  static Static_object<Asid_alloc> _asid_alloc;

  /**
   * Configure and enable the SMMU.
   *
   * \param base_addr   Base address of the SMMU.
   * \param eventq_irq  Event queue interrupt.
   * \param gerror_irq  Global error interrupt.
   */
  void setup(Address base_addr, unsigned eventq_irq, unsigned gerror_irq);

public:
  Iommu() :
    _idx(this - iommus().begin())
  {}
};

/**
 * Wrapper for a DMA space, which Iommu uses to associate internal state with
 * the DMA space.
 *
 * Can be bound to multiple Iommus, also with multiple stream IDs.
 */
class Iommu_domain
{
public:
  Iommu_domain() = default;
  ~Iommu_domain();

  Iommu_domain(Iommu_domain const &) = delete;
  Iommu_domain operator = (Iommu_domain const &) = delete;
  Iommu_domain(Iommu_domain &&) = delete;
  Iommu_domain operator = (Iommu_domain &&) = delete;

  Iommu::Asid get_asid() const { return Iommu::_asid_alloc->get_id(&_asid); }
  Iommu::Asid get_or_alloc_asid() { return Iommu::_asid_alloc->get_or_alloc_id(&_asid); }

private:
  friend class Iommu;

  enum : Unsigned32
  {
    Max_bindings_per_iommu = ~0U,
  };

  /**
   * Check whether domain is bound on the given Iommu.
   *
   * \pre The lock of iommu must be held.
   */
  bool is_bound(Iommu const *iommu) const;

  /**
   * Adds a binding for the given Iommu.
   *
   * \pre The lock of iommu must be held.
   */
  bool add_binding(Iommu const *iommu);

  /**
   * Deletes a binding for the given Iommu.
   *
   * \pre The lock of iommu must be held.
   */
  Unsigned32 del_binding(Iommu const *iommu);

  // Address space identifier of this domain. Allocated when the domain is
  // bound for the first time, freed only on destruction.
  Iommu::Asid _asid = Iommu::Invalid_asid;

  /**
   * Track on which SMMUs the domain was bound for how many stream IDs.
   *
   * The counters of a domain can be greater than the number of actual existing
   * bindings, namely when a binding of the domain was overbound with a
   * different domain. Generally, this overestimation is not a problem, but may
   * result in additional overhead in the following operations:
   *   - Flushing the TLB for the domain might execute a TLB flush on an SMMU
   *     the domain is no longer bound to.
   *   - `Iommu::remove()` has to scan the entire stream table, because the
   *     `count == 0` early return condition is not satisfied.
   */
  Unsigned32 _bindings[Iommu::Num_iommus] = { 0 };

  // OPTIMIZE: For the case that domain is only bound for a single stream id?
  //           Storing that stream id, would allow us to skip iterating all stream
  //           ids on destroy.
};

// ------------------------------------------------------------------
INTERFACE [iommu && iommu_arm_smmu_v3 && !arm_iommu_stage2]:

EXTENSION class Iommu { enum { Stage2 = 0 }; };

EXTENSION class Iommu_domain
{
private:
  Iommu::Cd const *get_or_init_cd(unsigned ias, unsigned virt_addr_size,
                                  Address pt_phys_addr);

  /// Context descriptor, used for all bindings of this domain (across SMMUs
  /// and across stream IDs). Initialized when the domain is bound for the first
  /// time. The context descriptor is only required if the SMMU does not
  /// support stage 2 translation.
  Iommu::Cd _cd;
};

// ------------------------------------------------------------------
INTERFACE [iommu && iommu_arm_smmu_v3 && arm_iommu_stage2]:

EXTENSION class Iommu { enum { Stage2 = 1 }; };

// ------------------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "cpu.h"
#include "feature.h"

Static_object<Iommu::Asid_alloc> Iommu::_asid_alloc;

KIP_KERNEL_FEATURE("arm,smmu-v3");

/**
 * Send commands to the SMMU, i.e. write them to the command queue.
 *
 * \pre CPU lock must not be held, as otherwise queue errors can not be handled.
 *
 * \param cmds  The commands to send.
 *
 * \return The wait index for the submitted commands.
 */
PRIVATE template<unsigned N>
Iommu::Cmd_queue::Index
Iommu::send_cmds(Cmd const (&cmds)[N])
{
  auto g = lock_guard(_cmd_queue_lock);

  Wait_warn_timeout wait_warn_timeout;
  Cmd_queue::Index wait_index;
  for (Cmd cmd : cmds)
    while (EXPECT_FALSE(!_cmd_queue.write(cmd, &wait_index)))
      {
        // Allow preemption, then retry to write the command.
        g.reset();

        if (wait_warn_timeout.check_warn(Queue_warn_timeout_us))
          WARNX(Error, "IOMMU: Command write timed out!\n");

        // Unfortunately, we cannot wait here with WFE, because if the command
        // queue contains only non-sync commands, even though that is unlikely,
        // the SMMU will not send a WFE event.
        Proc::pause();

        g.lock(&_cmd_queue_lock);
      }

  // We are done modifying the command queue, thus release the lock.

  return wait_index;
}

/**
 * Send a command to the SMMU and wait for its completion.
 *
 * \pre CPU lock must not be held, as otherwise queue errors can not be handled.
 *
 * \param cmd  The command to send.
 *
 * \note Orders subsequent writes to the completion of the SYNC command.
 */
PRIVATE
void
Iommu::send_cmd_sync(Cmd const &cmd)
{
  Cmd sync_cmd = Cmd::sync(Cmd::Compl_signal_wfe);
  Cmd_queue::Index wait_index = send_cmds({cmd, sync_cmd});

  // Wait until SMMU consumed all commands including the sync command.
  wait_cmd_queue(wait_index);

  // Waiting for the SYNC command to finish is a control dependency that orders
  // subsequent writes, i.e. writes following hereafter cannot pass the wait, and
  // thus are not visible to the SMMU until the wait here is done!
}

/**
 * Wait until the SMMU has consumed all commands from the command queue up to
 * the given wait index.
 *
 * \pre CPU lock must not be held, as otherwise queue errors can not be handled.
 * \pre Awaited command must be a sync command with Compl_signal_wfe.
 *
 * \param wait_index  Index of the command to wait for.
 */
PRIVATE
void
Iommu::wait_cmd_queue(Cmd_queue::Index wait_index)
{
  Wait_warn_timeout wait_warn_timeout;
  // Has SMMU consumed all commands up to the wait index?
  while (!_cmd_queue.is_item_complete(wait_index))
    {
      if (wait_warn_timeout.check_warn(Queue_warn_timeout_us))
        WARNX(Error, "IOMMU: Command execution timed out!\n");

      if (_support_wfe)
        // Not only SMMU sync command completion, but also exception return sets
        // the WFE event of this CPU. So even if we are interrupted, by hardware
        // interrupt or timer interrupt, the WFE event will be set when
        // execution returns to us via eret.
        asm volatile("wfe");
      else
        Proc::pause();
    }
}

/**
 * Lookup and optionally allocate stream table entry for stream ID.
 *
 * \pre stream_id < num_of_stream_ids()
 *
 * \param stream_id  The stream ID to lookup.
 * \param alloc      Allocate second-level stream table.
 */
PRIVATE
Iommu::Ste *
Iommu::lookup_stream_table_entry(Unsigned32 stream_id, bool alloc)
{
  assert(stream_id < num_of_stream_ids());

  if (!_strtab_2level)
    // Locate stream table entry in linear stream table.
    return &_strtab.virt_ptr<Ste>()[stream_id];

  L1std &l1 = _strtab.virt_ptr<L1std>()[stream_id >> Stream_table_split];

  {
    auto g = lock_guard(_strtab_l2_alloc_lock);

    // No second-level table allocated!
    if (!l1.is_valid())
      {
        if (!alloc || _strtab_l2_alloc_cnt >= Stream_table_l2_alloc_limit)
          return nullptr;

        Mem_chunk l2 = Mem_chunk::alloc_zmem(Stream_table_l2_size * sizeof(Ste));
        if (!l2.is_valid())
          return nullptr;

        _strtab_l2_alloc_cnt++;
        // Zero initialized second-level stream table must be observable by the
        // SMMU before setting up the corresponding L1 stream table descriptor.
        make_observable(l2.virt_ptr<Ste>(),
                        l2.virt_ptr<Ste>() + Stream_table_l2_size);

        L1std new_l1;
        new_l1.span() = Stream_table_split + 1;
        new_l1.l2_ptr() = l2.phys_addr();
        // Store has to be atomic to ensure that the SMMU never sees the L1
        // stream table descriptor in an inconsistent state.
        atomic_store(&l1, new_l1);
        // Ensure L1 stream table descriptor is observable by the SMMU before
        // configuring any L2 stream table entry. As that always requires
        // invalidation commands, make_observable_before_cmd is sufficient here.
        make_observable_before_cmd(&l1, &l1 + 1);
      }
  }

  // Locate stream table entry in second-level stream table.
  Ste *l2 = reinterpret_cast<Ste *>(Mem_layout::phys_to_pmem(l1.l2_ptr()));
  return &l2[stream_id & Stream_table_l2_mask];
}

PRIVATE
Iommu::Ste *
Iommu::iter_ste_tables(Unsigned32 *stream_id, Unsigned32 *table_size) const
{
  if (!_strtab_2level)
    {
      *table_size = num_of_stream_ids();
      return *stream_id == 0 ? _strtab.virt_ptr<Ste>() : nullptr;
    }

  assert(*stream_id % Stream_table_l2_size == 0);
  while (*stream_id < num_of_stream_ids())
    {
      // Access must be atomic so that we never see the L1 stream table entry in
      // an inconsistent state. This relies on the fact that L2 stream tables,
      // once allocated, are never deallocated later on.
      L1std l1 = atomic_load(&_strtab.virt_ptr<L1std>()[*stream_id >> Stream_table_split]);
      if (l1.is_valid())
        {
          *table_size = Stream_table_l2_size;
          return reinterpret_cast<Ste *>(Mem_layout::phys_to_pmem(l1.l2_ptr()));
        }

      *stream_id += Stream_table_l2_size;
    }

  return nullptr;
}

PRIVATE
void
Iommu::invalidate_ste(Ste *ste, Unsigned32 stream_id)
{
  // Mark stream table entry as invalid and ensure the write is observable by
  // the SMMU.
  ste->v() = 0;
  make_observable_before_cmd(ste);
  // Issue stream table entry invalidation command and wait for completion.
  send_cmd_sync(Cmd::cfgi_ste(stream_id, true));
}

/**
 * Delete a binding from the domain and invalidate the SMMU TLB for the
 * domain's ASID if it was its last remaining binding.
 *
 * \return Whether the removed binding was the last remaining.
 */
PRIVATE
bool
Iommu::delete_binding_and_invalidate_tlb(Iommu_domain &domain)
{
  if (domain.del_binding(this) != 0)
    return false;

  // We just removed the last binding the domain had with this SMMU. Flush the
  // ASID from the TLB (because if a domain is not bound to the SMMU, we don't
  // perform TLB flushes). This ensures that if the domain's ASID is later
  // rebound on this SMMU, no outdated TLB entries exist.
  tlb_invalidate_asid(domain.get_asid());
  return true;
}

/**
 * Ensure that all changes are observable by the SMMU (must be followed by
 * another write operation which is observed by the SMMU).
 */
PRIVATE template<typename T> static inline
void
Iommu::make_observable(T *start, T *end = nullptr)
{
  if (Iommu::Coherent)
    Mem::wmb(); // dmbist
  else
    Mem_unit::flush_dcache(start, end != nullptr ? end : start + 1);
}

/**
 * Ensure the given memory area is observable by the SMMU before subsequently
 * issued commands.
 */
PRIVATE template<typename T> static inline
void
Iommu::make_observable_before_cmd(T *start, T *end = nullptr)
{
  // Putting the command into the command queue already includes a memory
  // barrier in case the SMMU is cache coherent, so we only have to act in the
  // non-coherent case.
  if (!Iommu::Coherent)
    make_observable(start, end);
}

/**
 * Bind DMA domain to stream ID.
 *
 * \pre The arguments `virt_addr_size`, `start_level` and `pt_phys_addr` must
 *      stay constant per domain.
 *
 * \param stream_id       Stream ID to bind the DMA domain to.
 * \param domain          DMA domain to bind.
 * \param pt_phys_addr    Physical address of DMA domain's page table.
 * \param virt_addr_size  Size of the DMA virtual address size.
 * \param start_level     Walk start level of DMA domain's page table
 *                        (only-relevant for stage 2 SMMU).
 *
 * \retval  0, on success.
 * \retval <0, on failure.
 */
PUBLIC
int
Iommu::bind(Unsigned32 stream_id, Iommu_domain &domain, Address pt_phys_addr,
            unsigned virt_addr_size, unsigned start_level)
{
  auto g = lock_guard(_lock);

  if (stream_id >= num_of_stream_ids())
    return -L4_err::ERange;

  Ste *ste = lookup_stream_table_entry(stream_id, true);
  if (!ste)
    return -L4_err::ENomem;

  if (!domain.add_binding(this))
    return -L4_err::ENomem;

  if (!configure_domain(ste, stream_id, domain, pt_phys_addr,
                        virt_addr_size, start_level))
    {
      delete_binding_and_invalidate_tlb(domain);
      return -L4_err::ENomem;
    }

  return 0;
}

/**
 * Unbind DMA domain from stream ID.
 *
 * \param stream_id       Stream ID to unbind the DMA domain from.
 * \param domain          DMA domain to unbind.
 * \param pt_phys_addr    Physical address of DMA domain's page table.
 *
 * \retval  0, on success.
 * \retval <0, on failure.
 */
PUBLIC
int
Iommu::unbind(Unsigned32 stream_id, Iommu_domain &domain, Address pt_phys_addr)
{
  auto g = lock_guard(_lock);

  if (stream_id >= num_of_stream_ids())
    return -L4_err::ERange;

  Ste *ste = lookup_stream_table_entry(stream_id, false);
  if (ste && deconfigure_domain(ste, stream_id, domain, pt_phys_addr))
    delete_binding_and_invalidate_tlb(domain);

  // Regardless of whether the deconfiguration was successful, i.e. whether the
  // domain was bound to the stream ID or not, we always return success.
  return 0;
}

/**
 * Remove all stream ID bindings of this DMA domain.
 *
 * \param domain        DMA domain to remove.
 * \param pt_phys_addr  Physical address of DMA domain's page table.
 *
 * \note If new bindings are created concurrently, there might still exist
 *       bindings after remove returns. So if the caller relies on no bindings
 *       to exist afterwards, it must ensure that no new bindings can be created
 *       concurrently, e.g. by calling this from the destructor of Dmar_space).
 */
PUBLIC
void
Iommu::remove(Iommu_domain &domain, Address pt_phys_addr)
{
  auto g = lock_guard(_lock);

  if (!domain.is_bound(this))
    return;

  Unsigned32 base_stream_id = 0, table_size = 0;
  while (Ste *ste_table = iter_ste_tables(&base_stream_id, &table_size))
    {
      for (unsigned i = 0; i < table_size; i++)
        {
          Ste *ste = &ste_table[i];
          if (!deconfigure_domain(ste, base_stream_id + i, domain, pt_phys_addr))
            continue;

          if (delete_binding_and_invalidate_tlb(domain))
            // Optimization: Removed the last binding of this domain, we can
            // stop the scan here!
            return;
        }

      base_stream_id += table_size;
    }

  // After a domain has been removed with Iommu::remove() it can be deleted
  // afterwards. But if the domain's binding count is out-of-sync,
  // delete_binding_and_invalidate_tlb() call in the above loop never executes a
  // TLB flush. So to ensure that no entries with the domain's ASID, which gets
  // freed for reallocation once the domain is deleted, remain in the SMMU's
  // TLB, we need to execute a TLB flush here.
  tlb_invalidate_asid(domain.get_asid());

  // OPTIMIZE: Execute only one SYNC in the end to wait for completion of all
  //           invalidations?
}

/**
 * Setup linear stream table.
 */
PRIVATE
void
Iommu::setup_strtab_linear()
{
  unsigned strtab_size = num_of_stream_ids() * sizeof(Ste);
  _strtab = Mem_chunk::alloc_zmem(strtab_size, Strtab_base::align(strtab_size));
  if (!_strtab.is_valid())
    panic("IOMMU: Failed to allocate stream table.");

  Strtab_base strtab_base;
  strtab_base.addr() = _strtab.phys_addr();
  strtab_base.ra() = 1;
  write_reg(strtab_base);

  Strtab_base_cfg strtab_base_cfg;
  strtab_base_cfg.log2size() = _num_stream_id_bits;
  strtab_base_cfg.fmt() = Strtab_base_cfg::Fmt_linear;
  write_reg(strtab_base_cfg);

  _strtab_2level = false;
}

/**
 * Setup first level of two-level stream table.
 */
PRIVATE
void
Iommu::setup_strtab_2level()
{
  if (_num_stream_id_bits > Stream_table_max_bits)
    {
      WARN("IOMMU: Stream table covers only %u of stream ID's %u bits.\n",
           Stream_table_max_bits, _num_stream_id_bits);
      // Reduce number of stream IDs to the maximum covered by the stream table.
      _num_stream_id_bits = Stream_table_max_bits;
    }

  unsigned strtab_bits = _num_stream_id_bits - Stream_table_split;
  unsigned strtab_size = 1 << (strtab_bits) * sizeof(L1std);
  _strtab = Mem_chunk::alloc_zmem(strtab_size, Strtab_base::align(strtab_size));
  if (!_strtab.is_valid())
    panic("IOMMU: Failed to allocate stream table.");

  Strtab_base strtab_base;
  strtab_base.addr() = _strtab.phys_addr();
  strtab_base.ra() = 1;
  write_reg(strtab_base);

  Strtab_base_cfg strtab_base_cfg;
  strtab_base_cfg.log2size() = strtab_bits;
  strtab_base_cfg.split() = Stream_table_split;
  strtab_base_cfg.fmt() = Strtab_base_cfg::Fmt_2level;
  write_reg(strtab_base_cfg);

  _strtab_2level = true;
}

IMPLEMENT
void
Iommu::setup(Address base_addr, unsigned eventq_irq, unsigned gerror_irq)
{
  _rp0 = Mmio_register_block(base_addr);
  _rp1 = Mmio_register_block(offset_cast<void *>(base_addr, 0x10000));

  // Obtain hardware configuration from identification registers.
  Idr0 idr0 = read_reg<Idr0>();
  Idr1 idr1 = read_reg<Idr1>();
  Idr5 idr5 = read_reg<Idr5>();

  if (idr0.cohacc() != Iommu::Coherent)
    WARN("IOMMU: Configured as %sdma-coherent, "
         "but SMMU reports that coherent page table walks are %ssupported.\n",
         Iommu::Coherent ? "" : "non-", idr0.cohacc() ? "" : "un");

  if (idr0.ttf() != Idr0::Ttf_aarch64 && idr0.ttf() != Idr0::Ttf_aarch32_aarch64)
    panic("IOMMU: Aarch64 page table format not supported.");

  if (idr0.stall_model() == Idr0::Stall_model_forced)
    panic("IOMMU: Stall model that forces faults to stall is not supported.");

  if (idr1.queues_preset() || idr1.tables_preset())
    panic("IOMMU: Fixed queue or table base addresses are not supported!");

  _oas = address_size(idr5.oas());
  // On QEMU with Fiasco in the ARM_PT48 configuration the assumption that the
  // SMMU supports the same physical address size as the MMU does not hold (with
  // `cpu max` the physical address size is 48-bit, but SMMU still only supports
  // 44-bit).
  if (_oas < Cpu::phys_bits())
    // Unless too high physical addresses are mapped, the SMMU is going to work
    // fine, so don't panic/disable the SMMU, only print a warning.
    WARN("IOMMU: Supported physical address size smaller than required: %u\n",
         _oas);
  else
    // Otherwise set the output address size to the one used by Fiasco, which
    // might be lower than the maximum supported.
    _oas = Cpu::phys_bits();

  // The maximum intermediate address size is equals to the maximum output
  // address size (physical address size).
  _ias = _oas;

  _support_wfe = idr0.sev();

  if (Iommu::Debug)
    {
      printf("IOMMU: IDR0 s2p=%d s1p=%d ttf=%d cohacc=%d btm=%d asid16=%d "
             "msi=%d sev=%d vmid16=%d stall_model=%d st_level=%d\n",
             idr0.s2p().get(), idr0.s1p().get(), idr0.ttf().get(),
             idr0.cohacc().get(), idr0.btm().get(), idr0.asid16().get(),
             idr0.msi().get(), idr0.sev().get(), idr0.vmid16().get(),
             idr0.stall_model().get(), idr0.st_level().get());

      printf("IOMMU: IDR1 sidsize=%d ssidsize=%d eventqs=%d cmdqs=%d ecmdq=%d\n",
             idr1.sidsize().get(), idr1.ssidsize().get(), idr1.eventqs().get(),
             idr1.cmdqs().get(), idr1.ecmdq().get());

      printf("IOMMU: IDR5 oas=%d gran4k=%d vax=%d\n",
             idr5.oas().get(), idr5.gran4k().get(), idr5.vax().get());
    }

  if (Iommu::Stage2)
    {
      if (!idr0.s2p())
          panic("IOMMU: SMMU does not support stage 2 translation.");

      _num_asid_bits = idr0.vmid16() ? 16 : 8;
    }
  else
    {
      if (!idr0.s1p())
          panic("IOMMU: SMMU does not support stage 1 translation.");

      _num_asid_bits = idr0.asid16() ? 16 : 8;
    }

  if (idr0.msi())
    {
      Gerror_irq_cfg0 gerror_irq_cfg0;
      gerror_irq_cfg0.addr() = 0; // Use wired IRQ instead of MSI.
      write_reg(gerror_irq_cfg0);

      Eventq_irq_cfg0 eventq_irq_cfg0;
      eventq_irq_cfg0.addr() = 0; // Use wired IRQ instead of MSI.
      write_reg(eventq_irq_cfg0);
    }

  setup_gerror_handler(gerror_irq);
  setup_event_queue(idr1.eventqs(), eventq_irq);

  // Enable IRQs
  Irq_ctrl irq_ctrl;
  irq_ctrl.gerror_irqen() = Iommu::Debug;
  irq_ctrl.eventq_irqen() = Iommu::Log_faults;
  write_reg(irq_ctrl);
  // Wait for the changes to complete.
  while (irq_ctrl.raw != read_reg<Irq_ctrlack>().raw)
    Proc::pause();


  // Setup memory attributes for the SMMU's memory accesses to tables and
  // queues.
  Cr1 cr1;
  cr1.queue_ic() = cr1.queue_oc() = Cr1::Cache_wb;
  cr1.queue_sh() = Cr1::Share_is;
  cr1.table_ic() = cr1.table_oc() = Cr1::Cache_wb;
  cr1.table_sh() = Cr1::Share_is;
  write_reg(cr1);

  // Allocate tables and queues.
  _num_stream_id_bits = idr1.sidsize();
  // Use linear stream table if 2-level table format is not supported or linear
  // table does not span more than two 4k pages.
  if (idr0.st_level() != Strtab_base_cfg::Fmt_2level || _num_stream_id_bits <= 7)
    setup_strtab_linear();
  else
    setup_strtab_2level();

  unsigned cmdq_bits = min<unsigned>(Cmd_queue_max_bits, idr1.cmdqs().get());
  _cmd_queue.init(this, cmdq_bits);

  // Does the SMMU support broadcast TLB maintenance?
  if (idr0.btm())
    {
      // If yes, disable SMMU's participation in broadcast TLB maintenance, as
      // we don't share translation tables between PEs and SMMUs.
      Cr2 cr2;
      cr2.ptm() = 1;
      write_reg(cr2);
    }

  // Setup CR0
  Cr0 cr0;
  // Enable the SMMU
  cr0.smmuen() = 1;
  // Enable event queue if fault logging is enabled.
  cr0.eventqen() = Iommu::Log_faults;
  // Disable PRI queue.
  cr0.priqen() = 0;
  // Enable command queue.
  cr0.cmdqen() = 1;
  write_reg(cr0);
  // Wait for the changes to complete.
  while (cr0.raw != read_reg<Cr0ack>().raw)
    Proc::pause();

  // Flush all entries from the SMMU's TLB.
  tlb_flush();

  register_iommu_tlb();
}

/**
 * Invalidate TLB for domain.
 */
IMPLEMENT
void
Iommu::tlb_invalidate_domain(Iommu_domain const &domain)
{
  for (Iommu &iommu : Iommu::iommus())
    {
      auto g = lock_guard(iommu._lock);
      // Only invalidate TLB for the SMMUs this domain is bound to.
      if (domain.is_bound(&iommu))
        {
          auto asid = domain.get_asid();
          // Release IOMMU lock, as executing an invalidation command does not
          // require holding the lock.
          g.reset();
          iommu.tlb_invalidate_asid(asid);
        }
    }
}

IMPLEMENT_OVERRIDE
void
Iommu::init_common()
{
  if (!Iommu::iommus().size())
    return;

  unsigned max_asids = Max_nr_asid;
  for (Iommu &iommu : iommus())
    {
      max_asids = min(max_asids, 1u << iommu._num_asid_bits);

      if (iommu._ias != Iommu::iommus()[0]._ias)
        // If the assumption of a common IAS across all SMMUs does not hold,
        // we have to use a separate context descriptor per SMMU.
        panic("IOMMU: Intermediate address size differs between IOMMUs: %u vs. %u\n",
              Iommu::iommus()[0]._ias, iommu._ias);
    }

  if (Iommu::Debug)
    printf("IOMMU: %u ASIDs available.\n", max_asids);
  _asid_alloc.construct(max_asids);
}

IMPLEMENT
Iommu_domain::~Iommu_domain()
{
  Iommu::_asid_alloc->free_id_if_valid(&_asid);
}

IMPLEMENT
void
Iommu::tlb_flush()
{
  send_cmd_sync(Cmd::tlbi_nsnh_all());
}

IMPLEMENT inline
bool
Iommu_domain::is_bound(Iommu const *iommu) const
{
  return atomic_load(&_bindings[iommu->_idx]) > 0;
}

IMPLEMENT inline
bool
Iommu_domain::add_binding(Iommu const *iommu)
{

  if (EXPECT_FALSE(_bindings[iommu->_idx] >= Max_bindings_per_iommu))
    return false;

  atomic_store(&_bindings[iommu->_idx], _bindings[iommu->_idx] + 1);
  return true;
}

IMPLEMENT inline
Unsigned32
Iommu_domain::del_binding(Iommu const *iommu)
{
  if (EXPECT_TRUE(_bindings[iommu->_idx] > 0))
    {
      auto new_count = _bindings[iommu->_idx] - 1;
      atomic_store(&_bindings[iommu->_idx], new_count);
      return new_count;
    }
  else
    {
      WARNX(Error, "IOMMU: Attempt to delete binding while no bindings exist.");
      return 0;
    }
}

PRIVATE
bool
Iommu::configure_domain(Ste *ste, Unsigned32 stream_id, Iommu_domain &domain,
                        Address pt_phys_addr, unsigned virt_addr_size,
                        unsigned start_level)
{
  // Stream table entry is already valid, invalidate it first to ensure the SMMU
  // does not see an illegal/inconsistent intermediate state.
  if (ste->v())
    invalidate_ste(ste, stream_id);

  // 1. Initialize STE, but do not set the valid flag for now.
  if (!prepare_ste(ste, domain, pt_phys_addr, virt_addr_size, start_level))
    return false;

  // 2. Ensure the STE is observable by the SMMU.
  make_observable_before_cmd(ste);
  // 3. Issue STE invalidation command and wait for completion.
  send_cmd_sync(Cmd::cfgi_ste(stream_id, true));

  // 4. Mark STE as valid and ensure it's observable by the SMMU.
  ste->v() = 1;
  make_observable_before_cmd(ste);

  // 5. Issue STE invalidation command and wait for completion.
  send_cmd_sync(Cmd::cfgi_ste(stream_id, true));

  return true;
}

PRIVATE
bool
Iommu::deconfigure_domain(Ste *ste, Unsigned32 stream_id, Iommu_domain &domain,
                          Address pt_phys_addr)
{
  // Stream table entry is invalid.
  if (!ste->v())
    return false;

  // Stream table entry is bound to a different domain.
  if (!is_domain_bound_to_ste(ste, domain, pt_phys_addr))
    return false;

  invalidate_ste(ste, stream_id);
  return true;
}

// -----------------------------------------------------------
IMPLEMENTATION [iommu && iommu_arm_smmu_v3 && !arm_iommu_stage2]:

IMPLEMENT
Iommu::Cd const *
Iommu_domain::get_or_init_cd(unsigned ias, unsigned virt_addr_size, Address pt_phys_addr)
{
  // Already initialized?
  if (_cd.v())
    return &_cd;

  unsigned long asid = get_or_alloc_asid();
  if (asid == Iommu::Invalid_asid)
    // No ASID available.
    return nullptr;

  // Region size is 2^(64 - T0SZ) -> T0SZ = 64 - input_address_size
  _cd.t0sz() = 64 - virt_addr_size;
  _cd.tg0() = 0; // 4k
  _cd.ir0() = Iommu::Cr1::Cache_wb;
  _cd.or0() = Iommu::Cr1::Cache_wb;
  _cd.sh0() = Iommu::Cr1::Share_is;
  _cd.epd0() = 0; // Enable TT0 translation table walk.
  _cd.endi() = 0; // Translation table endianness is little endian.
  _cd.epd1() = 1; // Disable TT1 translation table walk.
  _cd.v() = 1; // Valid
  _cd.ips() = Iommu::address_size_encode(ias);
  _cd.affd() = 1; // An Access flag fault never occurs.
  _cd.wxn() = 0;
  _cd.uwxn() = 0;
  _cd.aa64() = 1; // Use Aarch64 format
  _cd.hd() = 0; // Disable hardware update of Dirty flags
  _cd.ha() = 0; // Disable hardware update of Access flags
  // Fault behavior
  _cd.s() = 0; // Do not stall faulting transactions.
  _cd.r() = Iommu::Log_faults; // Record faults in event queue.
  _cd.a() = 1; // Translation faults result in an abort or bus error being
               // returned to the device.
  // The context is non-shared, do not participate in broadcast TLB.
  _cd.aset() = 1;
  _cd.asid() = asid;
  _cd.ttb0() = pt_phys_addr;
  _cd.mair0() = Page::Mair0_prrr_bits;
  _cd.mair1() = Page::Mair1_nmrr_bits;
  _cd.amair0() = 0;
  _cd.amair1() = 0;

  // Ensure context descriptor is observable by the SMMU.
  Iommu::make_observable_before_cmd(&_cd);

  return &_cd;
}

PRIVATE
void
Iommu::tlb_invalidate_asid(Asid asid)
{
  // When domain is not bound it has the invalid ASID.
  if (asid != Invalid_asid)
    send_cmd_sync(Cmd::tlbi_nh_asid(Default_vmid, asid));
}

PRIVATE
bool
Iommu::prepare_ste(Ste *ste, Iommu_domain &domain, Address pt_phys_addr,
                   unsigned virt_addr_size, unsigned)
{
  // Get or allocate context descriptor.
  Cd const *cd = domain.get_or_init_cd(_ias, virt_addr_size, pt_phys_addr);
  if (!cd)
    return false;

  // Initialize stream table entry.
  *ste = Ste();
  ste->v() = 0;
  ste->config() = Ste::Config_enable | Ste::Config_s1_translate;
  // S1CDMax is set to zero (no substreams), thus S1Fmt is ignored and
  // S1ContextPtr points to single context descriptor.
  ste->s1_context_ptr() = Mem_layout::pmem_to_phys(cd);
  ste->s1_cir() = Cr1::Cache_wb;
  ste->s1_cor() = Cr1::Cache_wb;
  ste->s1_csh() = Cr1::Share_is;

  // StreamWorld = NS-EL1, tagged with both ASID+VMID
  ste->strw() = 0;
  // Use incoming Shareability attribute
  ste->shcfg() = 1;

  // VMID is also used when only stage 1 translation is configured, we use the
  // same VMID for all stream table entries.
  ste->s2_vmid() = Default_vmid;

  return true;
}

PRIVATE inline
bool
Iommu::is_domain_bound_to_ste(Ste *ste, Iommu_domain const &,
                              Address pt_phys_addr) const
{
  Cd *cd = reinterpret_cast<Cd *>(Mem_layout::phys_to_pmem(ste->s1_context_ptr()));
  // Valid context descriptor pointer referring to the domain's page table.
  return cd && cd->ttb0() == pt_phys_addr;
}

// -----------------------------------------------------------
IMPLEMENTATION [iommu && iommu_arm_smmu_v3 && arm_iommu_stage2]:

PRIVATE
void
Iommu::tlb_invalidate_asid(Asid asid)
{
  // When domain is not bound, it has the invalid ASID.
  if (asid != Invalid_asid)
    send_cmd_sync(Cmd::tlbi_s12_vmall(asid));
}

PRIVATE
bool
Iommu::prepare_ste(Ste *ste, Iommu_domain &domain, unsigned virt_addr_size,
                   unsigned start_level, Address pt_phys_addr)
{
  unsigned long vmid = domain.get_or_alloc_asid();
  if (vmid == Invalid_asid)
    // No VMID available.
    return false;

  // Initialize stream table entry.
  *ste = Ste();
  ste->v() = 0;
  ste->config() = Ste::Config_enable | Ste::Config_s2_translate;

  // Use incoming Shareability attribute
  ste->shcfg() = 1;

  ste->s2_vmid() = vmid;
  // Region size is 2^(64 - T0SZ) -> T0SZ = 64 - input_address_size
  ste->s2_t0sz() = 64 - virt_addr_size;
  ste->s2_sl0() = start_level;
  ste->s2_ir0() = Cr1::Cache_wb;
  ste->s2_or0() = Cr1::Cache_wb;
  ste->s2_sh0() = Cr1::Share_is;
  ste->s2_tg() = 0; // 4k
  ste->s2_ps() = address_size_encode(_oas);
  ste->s2_aa64() = 1; // Use Aarch64 format
  ste->s2_affd() = 1; // An Access flag fault never occurs.
  ste->s2_s() = 0; // Do not stall faulting transactions.
  ste->s2_r() = Iommu::Log_faults; // Record faults in event queue.
  ste->s2_ttb() = pt_phys_addr;

  return true;
}

PRIVATE inline
bool
Iommu::is_domain_bound_to_ste(Ste *ste, Iommu_domain const &,
                              Address pt_phys_addr) const
{
  // Stream table entry refers to the domain's page table.
  return ste->s2_ttb() == pt_phys_addr;
}

// ------------------------------------------------------------------
IMPLEMENTATION [iommu]:

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
  check(Irq_mgr::mgr->alloc(irq, pin));
  // "Interrupts in SMMUv3 are required to be edge-triggered or MSIs."
  irq->chip()->set_mode(irq->pin(), Irq_chip::Mode::F_raising_edge);
  irq->unmask();
}

}

PRIVATE
void
Iommu::handle_cmdq_error()
{
  Cmdq_cons cmdq_cons = read_reg<Cmdq_cons>();
  WARNX(Error, "IOMMU: Global error CMDQ_ERR occured: Err=%u Index=%u\n",
        cmdq_cons.err().get(), cmdq_cons.rd().get());

  switch (cmdq_cons.err())
    {
    case Cmdq_cons::Err_none:
      // No error, should never occur.
      return;
    case Cmdq_cons::Err_ill:
      // Command illegal, replace command with nop.
      break;
    case Cmdq_cons::Err_abt:
      // Abort on command fetch, retry.
      WARNX(Error, "IOMMU: Retrying command fetch.\n");
      return;
    case Cmdq_cons::Err_atc_inv_sync:
      // ATC invalidation timeout, repeat the sync command with the assumption
      // that the ATC invalidation will complete eventually.
      // Should never occur as we do not use ATC.
      return;
    default:
      // Unknown error, replace command with nop.
      break;
    }

  Cmd *slot = _cmd_queue.slot_at(cmdq_cons.rd());
  WARNX(Error, "IOMMU: Skipping illegal command:\n"
               "  [  0: 63] 0x%016llx\n"
               "  [ 64:127] 0x%016llx\n",
               slot->raw[0], slot->raw[1]);

  // Use sync command as nop-like replacement for illegal command.
  *slot = Cmd::sync(Cmd::Compl_signal_wfe);
  make_observable(slot);
}

PRIVATE
void
Iommu::handle_gerror_irq()
{
  Gerror gerror = read_reg<Gerror>();
  Gerrorn gerrorn = read_reg<Gerrorn>();

  Gerror active = Gerror::from_raw(gerror.raw ^ gerrorn.raw);
  if (!active.raw)
    // No active error conditions.
    return;

  if (active.cmdq_err())
    handle_cmdq_error();
  if (active.eventq_abt_err())
    WARNX(Error, "IOMMU: Global error EVENTQ_ABT_ERR occured.\n");
  if (active.priq_abt_err())
    WARNX(Error, "IOMMU: Global error PRIQ_ABT_ERR occured.\n");
  if (active.msi_cmdq_abt_err())
    WARNX(Error, "IOMMU: Global error MSI_CMDQ_ABT_ERR occured.\n");
  if (active.msi_eventq_abt_err())
    WARNX(Error, "IOMMU: Global error MSI_EVENTQ_ABT_ERR occured.\n");
  if (active.msi_priq_abt_err())
    WARNX(Error, "IOMMU: Global error MSI_PRIQ_ABT_ERR occured.\n");
  if (active.msi_gerror_abt_err())
    WARNX(Error, "IOMMU: Global error MSI_GERROR_ABT_ERR occured.\n");
  if (active.sfm_err())
    WARNX(Error, "IOMMU: Global error SFM_ERR occured.\n");
  if (active.cmdqp_err())
    WARNX(Error, "IOMMU: Global error CMDQP_ERR occured.\n");

  // Acknowledge error conditions.
  write_reg<Gerrorn>(gerror.raw);
}

PRIVATE
void
Iommu::setup_gerror_handler(unsigned gerror_irq)
{
  setup_irq(gerror_irq, this, &Iommu::handle_gerror_irq);
}

// ------------------------------------------------------------------
IMPLEMENTATION [iommu && !debug]:

EXTENSION class Iommu
{
public:
  enum
  {
    Debug      = 0,
    Log_faults = 0,
  };

private:
  void setup_event_queue(unsigned, unsigned) {}
};

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

private:
  struct Event
  {
    Unsigned64 raw[4];

    CXX_BITFIELD_MEMBER_RO( 0,  7, type, raw[0]);
    CXX_BITFIELD_MEMBER_RO(32, 63, stream_id, raw[0]);

    enum Type
    {
      F_uut              = 0x01,
      C_bad_streamid     = 0x02,
      F_ste_fetch        = 0x03,
      C_bad_ste          = 0x04,
      F_bad_ats_treq     = 0x05,
      F_stream_disabled  = 0x06,
      F_transl_forbidden = 0x07,
      C_bad_substreamid  = 0x08,
      F_cd_fetch         = 0x09,
      C_bad_cd           = 0x0a,
      F_walk_eabt        = 0x0b,
      F_translation      = 0x10,
      F_addr_size        = 0x11,
      F_access           = 0x12,
      F_permission       = 0x13,
      F_tlb_conflict     = 0x20,
      F_cfg_conflict     = 0x21,
      E_page_request     = 0x24,
      F_vms_fetch        = 0x25,
    };

    char const *type_str() const
    { return type_as_str(static_cast<Type>(type().get())); }

    static char const *type_as_str(Type type)
    {
      switch (type)
        {
        case F_uut: return "F_UUT";
        case C_bad_streamid: return "C_BAD_STREAMID";
        case F_ste_fetch: return "F_STE_FETCH";
        case C_bad_ste: return "C_BAD_STE";
        case F_bad_ats_treq: return "F_BAD_ATS_TREQ";
        case F_stream_disabled: return "F_STREAM_DISABLED";
        case F_transl_forbidden: return "F_TRANSL_FORBIDDEN";
        case C_bad_substreamid: return "C_BAD_SUBSTREAMID";
        case F_cd_fetch: return "F_CD_FETCH";
        case C_bad_cd: return "C_BAD_CD";
        case F_walk_eabt: return "F_WALK_EABT";
        case F_translation: return "F_TRANSLATION";
        case F_addr_size: return "F_ADDR_SIZE";
        case F_access: return "F_ACCESS";
        case F_permission: return "F_PERMISSION";
        case F_tlb_conflict: return "F_TLB_CONFLICT";
        case F_cfg_conflict: return "F_CFG_CONFLICT";
        case E_page_request: return "E_PAGE_REQUEST";
        case F_vms_fetch: return "F_VMS_FETCH";
        }
      return "UNKNOWN";
    }
  };
  static_assert(sizeof(Event) == 32, "Event has unexpected size!");

  using Event_queue = Read_queue<Event, Eventq_base, Eventq_prod, Eventq_cons>;
  Event_queue _event_queue;
};

PRIVATE
void
Iommu::handle_eventq_irq()
{
  auto on_overflow = []()
  {
    printf("IOMMU: Event queue overflowed, events have been lost.\n");
  };

  Event event;
  while (_event_queue.read(event, on_overflow))
    {
      printf("IOMMU: Event %s (%u) occured for stream ID %u:\n"
             "  [  0: 63] 0x%016llx\n"
             "  [ 64:127] 0x%016llx\n"
             "  [128:191] 0x%016llx\n"
             "  [192:255] 0x%016llx\n",
             event.type_str(), event.type().get(), event.stream_id().get(),
             event.raw[0], event.raw[1], event.raw[2], event.raw[3]);
    }
}

PRIVATE
void
Iommu::setup_event_queue(unsigned supported_eventq_bits, unsigned eventq_irq)
{
  if (Iommu::Log_faults)
    {
      unsigned eventq_bits = min<unsigned>(supported_eventq_bits,
                                           Event_queue_max_bits);
      _event_queue.init(this, eventq_bits);

      setup_irq(eventq_irq, this, &Iommu::handle_eventq_irq);
    }
}
