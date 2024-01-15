INTERFACE:

#include "types.h"
#include <cxx/bitfield>

// Common constants also required if we disable ITS.
class Gic_its
{
public:
  enum
  {
    GITS_CTLR         = 0x0000,
    GITS_IIDR         = 0x0004,
    GITS_TYPER        = 0x0008,
    GITS_STATUSR      = 0x0040,
    GITS_UMSIR        = 0x0048,
    GITS_CBASER       = 0x0080,
    GITS_CWRITER      = 0x0088,
    GITS_CREADR       = 0x0090,
    GITS_BASER        = 0x0100,
    GITS_PIDR2        = 0xffe8,
    GITS_ITS_BASE     = 0x10000,
    GITS_TRANSLATER   = GITS_ITS_BASE + 0x0040,

    GITS_baser_num = 8,

    GITS_cmd_queue_align       = 0x10000,
    GITS_cmd_queue_size        = GITS_cmd_queue_align,
    GITS_cmd_queue_page_size   = 0x1000,
    GITS_cmd_queue_entry_size  = 32,
    GITS_cmd_queue_offset_mask = 0xfffe0,
  };

  struct Ctlr
  {
    Unsigned32 raw;
    explicit Ctlr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER   ( 0,  0, enabled, raw);
    CXX_BITFIELD_MEMBER   ( 8,  8, umsi_irq, raw);
    CXX_BITFIELD_MEMBER_RO(31, 31, quiescent, raw);
  };
};

// ------------------------------------------------------------------------
INTERFACE[arm_gic_msi]:

#include "boot_alloc.h"
#include "cpu.h"
#include "gic_cpu_v3.h"
#include "gic_dist.h"
#include "gic_mem.h"
#include "gic_redist.h"
#include "irq_mgr.h"
#include "kmem_slab.h"
#include "mmio_register_block.h"
#include "spin_lock.h"

#include <arithmetic.h>
#include <cxx/hlist>
#include "cxx/static_vector"

/**
 * The GICv3 architecture provides support for message-based interrupts, e.g.
 * Message Signaled Interrupts (MSI), with the help of the Interrupt
 * Translation Service (ITS) component.
 *
 * The kernel configures the ITS to map the combination of a DeviceID and an
 * EventID to a Locality-specific Peripheral Interrupt (LPI) directed to a
 * redistributor (i.e. to a CPU). To trigger an LPI, a device has to write the
 * corresponding EventID to the GITS_TRANSLATER register of the ITS. The
 * DeviceID for a write, which is unique for each device, is presented to the
 * ITS in a way that cannot be spoofed. Thus, the ITS can ensure that a device
 * can only trigger the LPIs that have been mapped for it.
 *
 * For the translation of MSIs (devices writing EventIDs to GITS_TRANSLATER)
 * into LPIs, the ITS uses memory tables provided by the kernel. However, the
 * kernel does not directly write to these tables, but instead configures the
 * ITS through commands issued via a command queue.
 *
 * Because there can be multiple ITSs in the system, which might each be
 * responsible for a different subset of devices, the assignment of an LPI
 * to an ITS is not decided until user space binds the corresponding MSI to
 * a device. Thus, LPIs are not managed inside a Gic_its object, but in the
 * MSI interrupt controller (Gic_msi).
 *
 * To avoid deadlocks, locks may only be grabbed in the following order:
 *   1. If necessary, grab an LPI lock.
 *   2. If necessary, grab the `_device_alloc_lock`.
 *   3. If necessary, grab the `_cmd_queue_lock`.
 */
EXTENSION class Gic_its
{
public:
  struct Typer
  {
    Unsigned64 raw;
    explicit Typer(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_RO( 4,  7, itt_entry_size, raw);
    CXX_BITFIELD_MEMBER_RO( 8, 12, id_bits, raw);
    CXX_BITFIELD_MEMBER_RO(13, 17, dev_bits, raw);
    CXX_BITFIELD_MEMBER_RO(19, 19, pta, raw);
    CXX_BITFIELD_MEMBER_RO(24, 31, hcc, raw);
  };

  struct Cbaser
  {
    Unsigned64 raw = 0;
    Cbaser() = default;
    explicit Cbaser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  7, size, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 51, pa, raw);
    CXX_BITFIELD_MEMBER          (59, 61, cacheability, raw);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw);
  };

  struct Baser
  {
    Unsigned64 raw = 0;
    Baser() = default;
    explicit Baser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  7, size, raw);
    CXX_BITFIELD_MEMBER          ( 8,  9, page_size, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 47, pa, raw);
    CXX_BITFIELD_MEMBER_RO       (48, 52, entry_size, raw);
    CXX_BITFIELD_MEMBER_RO       (56, 58, type, raw);
    CXX_BITFIELD_MEMBER          (59, 61, cacheability, raw);
    CXX_BITFIELD_MEMBER          (62, 62, indirect, raw);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw);

    enum Type
    {
      Type_none       = 0,
      Type_device     = 1,
      Type_vpe        = 2,
      Type_collection = 4,
    };

    enum
    {
      Page_size_4k     = 0,
      Page_size_16k    = 1,
      Page_size_64k    = 2,

      Size_max        = 256,
    };
  };

  struct L1_entry
  {
    Unsigned64 raw = 0;
    L1_entry() = default;
    explicit L1_entry(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 51, pa, raw);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw);

    enum { Size = 8 };
  };

  using Dev_id = unsigned;
  using Event_id = unsigned;
  using Icid = unsigned;

  enum
  {
    // One collection for each CPU (redistributor).
    Num_cols     = Config::Max_num_cpus,
    Invalid_icid = Num_cols + 1,
  };

  struct Rd_base
  {
    Unsigned64 raw = 0;

    CXX_BITFIELD_MEMBER_UNSHIFTED(16, 50, phys_base_addr, raw);
    CXX_BITFIELD_MEMBER          (16, 31, processor_nr, raw);
  };

  class Table
  {
  public:
    using Reg = Mmio_register_block::Reg_t<Unsigned64>;
    enum
    {
      // Arbitrarily chosen threshold for using two-level tables (64 KiB).
      Max_direct_size = 0x10000,
    };

    inline bool is_valid() const
    { return _mem.is_valid(); }

    inline Gic_mem const &mem() const
    { return _mem; }

    void alloc(Reg r, Typer typer);
    bool ensure_id_present(unsigned id);

    inline Baser::Type type() const
    { return _type; }

  private:
    Baser::Type _type = Baser::Type_none;
    Gic_mem _mem;
    unsigned _page_size = 0;
    unsigned _entry_size = 0;
    bool _indirect = false;
  };

  enum
  {
    Max_num_devs = 64,
  };

  class Device;
  struct Collection
  {
    Icid icid = Invalid_icid;
    Rd_base redist_base;

    inline bool is_valid() const
    { return icid != Invalid_icid; }
  };

  /**
   * LPI base class that contains all the state of an LPI relevant for the
   * ITS, and from which the MSI interrupt controller (Gic_msi) derives its LPI state
   * representation class.
   */
  class Lpi
  {
  public:
    Lpi() { reset(); }

    // Index of this LPI, assigned by the MSI interrupt controller.
    unsigned index;
    // Coordinates concurrent operations on this LPI.
    mutable Spin_lock<> lock;

  private:
    friend class Gic_its;

    Device *device;
    Collection const *col;

    inline void reset()
    {
      device = nullptr;
      col = nullptr;
    }

    /**
     * We use a global EventID space, where the EventID of an LPI corresponds
     * to its index.
     *
     * With a per device EventID space we could save resources due to smaller
     * ITTs, but we would need a per device EventID allocation scheme, such as
     * a bitmap.
     * However, an ITT size tailored to the specific requirements of the device
     * is not possible because of the limitations of the Fiasco MSI userspace
     * API. Thus, the ITT size would decrease to the number of MSIs needed by
     * the device with the highest demand, instead of the sum of MSIs required
     * by all devices.
     */
    Event_id event_id() const { return index; }
    unsigned intid() const { return Gic_dist::Lpi_intid_base + index; }
  };

  class Device : public cxx::H_list_item
  {
  public:
    explicit Device(Dev_id id, Gic_its &its) : _id(id), _its(its) {}

    ~Device()
    {
      if (_itt.is_valid())
        free_itt();
    }

    bool setup_itt();
    void free_itt();
    unsigned itt_size();

    void bind_lpi(Lpi &lpi);
    void unbind_lpi(Lpi &lpi);

    inline Dev_id id()
    { return _id; }

    bool has_lpis()
    { return _lpi_count; }

  private:
    Dev_id _id;
    Gic_its &_its;
    Gic_mem _itt;
    unsigned _lpi_count = 0;
  };

  class Cmd
  {
  private:
    Unsigned64 raw[4] = { 0 };

  public:
    enum : unsigned { Size = GITS_cmd_queue_entry_size };

    enum Op
    {
      Op_movi    = 0x01,
      Op_sync    = 0x05,
      Op_mapd    = 0x08,
      Op_mapc    = 0x09,
      Op_mapti   = 0x0a,
      Op_inv     = 0x0c,
      Op_invall  = 0x0d,
      Op_discard = 0x0f,
    };

    Cmd() = default;
    explicit Cmd(Op cmd_op) { op() = cmd_op; };

    CXX_BITFIELD_MEMBER          ( 0,  7, op, raw[0]);
    CXX_BITFIELD_MEMBER          (32, 63, dev_id, raw[0]);

    CXX_BITFIELD_MEMBER          ( 0, 31, event_id, raw[1]);
    CXX_BITFIELD_MEMBER          (32, 63, intid, raw[1]);
    CXX_BITFIELD_MEMBER          ( 0,  4, itt_size, raw[1]);

    CXX_BITFIELD_MEMBER          ( 0, 15, icid, raw[2]);
    CXX_BITFIELD_MEMBER_UNSHIFTED(16, 50, rd_base, raw[2]);
    CXX_BITFIELD_MEMBER_UNSHIFTED( 8, 51, itt_addr, raw[2]);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw[2]);

    /**
     * This command retargets an already mapped event to a different
     * redistributor.
     */
    static Cmd movi(Dev_id dev_id, Event_id event_id, Icid icid)
    {
      Cmd cmd(Op_movi);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      cmd.icid() = icid;
      return cmd;
    }

    /**
     * This command ensures that the effects of all previous physical commands
     * associated with the specified redistributor are globally observable.
     */
    static Cmd sync(Rd_base rd_base)
    {
      Cmd cmd(Op_sync);
      cmd.rd_base() = rd_base.raw;
      return cmd;
    }

    /**
     * This command maps a DeviceID to an interrupt translation table (ITT).
     */
    static Cmd mapd(Dev_id dev_id, Address itt_addr, unsigned itt_size)
    {
      Cmd cmd(Op_mapd);
      cmd.dev_id() = dev_id;
      if (itt_addr)
        {
          // Associate DeviceID with ITT
          cmd.itt_addr() = itt_addr;
          cmd.itt_size() = itt_size - 1;
          cmd.valid() = true;
        }
      else
        {
          // Remove association for DeviceID
          cmd.valid() = false;
        }
      return cmd;
    }

    /**
     * This command maps a collection to a redistributor.
     */
    static Cmd mapc(Icid icid, Rd_base rd_base, bool valid)
    {
      Cmd cmd(Op_mapc);
      cmd.icid() = icid;
      if (valid)
        {
          // Map ICID to target redistributor
          cmd.rd_base() = rd_base.raw;
          cmd.valid() = true;
        }
      else
        {
          // Remove mapping for ICID
          cmd.valid() = false;
        }
      return cmd;
    }

    /**
     * This command maps an event to an LPI targeted at the specified
     * redistributor.
     */
    static Cmd mapti(Dev_id dev_id, Event_id event_id, unsigned intid, Icid icid)
    {
      Cmd cmd(Op_mapti);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      cmd.intid() = intid;
      cmd.icid() = icid;
      return cmd;
    }

    /**
     * This command ensures that any caching done by the redistributors
     * associated with the specified event is consistent with the LPI
     * configuration tables held in memory.
     */
    static Cmd inv(Dev_id dev_id, Event_id event_id)
    {
      Cmd cmd(Op_inv);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      return cmd;
    }

    static Cmd invall(Icid icid)
    {
      Cmd cmd(Op_invall);
      cmd.icid() = icid;
      return cmd;
    }

    /**
     * This command removes the mapping for the specified event from the ITT and
     * resets the pending state of the corresponding LPI.
     */
    static Cmd discard(Dev_id dev_id, Event_id event_id)
    {
      Cmd cmd(Op_discard);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      return cmd;
    }
  };
  static_assert(sizeof(Cmd) == Cmd::Size, "Invalid size of Cmd");

private:
  Gic_cpu_v3 *_gic_cpu;
  Mmio_register_block _its;
  Spin_lock<> _lock;

  Collection _cols[Num_cols];
  bool _redist_pta;

  unsigned _num_lpis;

  Table _tables[GITS_baser_num];
  Table *_device_table;

  Gic_mem _cmd_queue;
  unsigned _cmd_queue_write_off;
  Spin_lock<> _cmd_queue_lock;

  unsigned _itt_entry_size;
  unsigned _max_device_id;

  using Device_list = cxx::H_list_bss<Device>;
  Device_list _devices;
  unsigned _num_devs;
  Spin_lock<> _device_alloc_lock;

  using Device_alloc = Kmem_slab_t<Device>;
  static Device_alloc device_alloc;
};

// ------------------------------------------------------------------------
IMPLEMENTATION[arm_gic_msi]:

#include "cpu.h"
#include "kmem_alloc.h"
#include "panic.h"
#include "poll_timeout_counter.h"
#include <cstdio>
#include <string.h>

Gic_its::Device_alloc Gic_its::device_alloc;

IMPLEMENT
void
Gic_its::Table::alloc(Reg r, Typer typer)
{
  Baser baser(r.read_non_atomic());

  _type = static_cast<Baser::Type>(baser.type().get());
  _entry_size = baser.entry_size() + 1;
  Unsigned64 size;
  switch (_type)
    {
    case Baser::Type_device:
      // One entry for each DeviceID.
      size = static_cast<Unsigned64>(_entry_size) << (typer.dev_bits() + 1);
      // If required memory size is above the threshold use a lazily populated
      // two-level device table to avoid wasting a lot of memory.
      _indirect = size > Max_direct_size;
      break;

    case Baser::Type_collection:
      if (Num_cols <= typer.hcc())
        {
          // ITS can store a sufficient amount of collection table entries in
          // internal memory, no need to allocate an external collection table.
          if (Warn::is_enabled(Info))
            printf("ITS: Using internal memory to store collections.\n");
          return;
        }

      // One entry for each redistributor.
      size = Num_cols * _entry_size;
      _indirect = false;
      break;

    case Baser::Type_vpe:
      // TODO: Direct VM LPI injection not yet supported...
      return;

    case Baser::Type_none:
      return;

    default:
      WARN("ITS: Skip allocation of table with unknown type=%u.\n", _type);
      return;
    }

  // Detect supported page size. We try to stick with 4k pages to keep the
  // alignment overhead low. But if the HW forces us to use larger pages so be
  // it...
  baser.page_size() = Baser::Page_size_4k;
  r.write_non_atomic(baser.raw);
  baser.raw = r.read_non_atomic();
  switch (baser.page_size())
    {
    case Baser::Page_size_4k:   _page_size =  0x1000; break;
    case Baser::Page_size_16k:  _page_size =  0x4000; break;
    default:                    _page_size = 0x10000; break;
    }

  if (_indirect)
    {
      unsigned entries = size / _entry_size;
      unsigned entries_per_page = _page_size / _entry_size;
      unsigned l2_pages = cxx::div_ceil(entries, entries_per_page);
      unsigned l1_pages = cxx::div_ceil(l2_pages, _page_size / L1_entry::Size);
      size = l1_pages * _page_size;
    }
  else
    {
      size = cxx::ceil_lsb(size, cxx::log2u(_page_size));
    }

  _mem = Gic_mem::alloc_zmem(size, _page_size);
  if (!_mem.is_valid())
    panic("ITS: Failed to allocate table of type=%u and size=0x%llx.\n",
          _type, size);

  unsigned num_pages = size / _page_size;
  assert(num_pages <= Baser::Size_max);
  baser.size() = num_pages - 1;
  // The physical bits 51:48 of the physical address must be zero!
  assert((_mem.phys_addr() & (0xfULL << 48)) == 0UL);
  baser.pa() = _mem.phys_addr();
  baser.indirect() = _indirect;
  baser.valid() = 1;
  _mem.setup_reg(r, baser);
  _mem.make_coherent();

  if (Warn::is_enabled(Info))
    printf("ITS: Allocated table of type=%u with size=0x%llx "
           "indirect=%u page_size=%u entry_size=%u pages=%u.\n",
           _type, size, _indirect, _page_size, _entry_size, num_pages);
}

IMPLEMENT
bool
Gic_its::Table::ensure_id_present(unsigned id)
{
  // Only two-level tables are lazily allocated.
  if (!_indirect)
    return true;

  unsigned l1_index = id / (_page_size / _entry_size);
  L1_entry *l1_table = _mem.virt_ptr<L1_entry>();
  L1_entry e = l1_table[l1_index];
  if (e.valid())
    // Second level table is already allocated.
    return true;

  Gic_mem l2_table = Gic_mem::alloc_zmem(_page_size, _page_size);
  if (l2_table.is_valid())
    {
      e.pa() = l2_table.phys_addr();
      e.valid() = 1;
      l2_table.inherit_mem_attribs(_mem);
      l2_table.make_coherent();

      L1_entry *ep = l1_table + l1_index;
      write_now(ep, e);
      _mem.make_coherent(ep, ep + 1);

      return true;
    }
  else
    {
      WARNX(Error, "ITS: Failed to allocate second-level table of type=%u.\n",
            _type);
      return false;
    }
}

PUBLIC
void
Gic_its::init(Gic_cpu_v3 *gic_cpu, Address base, unsigned num_lpis)
{
  _its = Mmio_register_block(base);
  unsigned arch_rev = (_its.read<Unsigned32>(GITS_PIDR2) >> 4) & 0xf;
  if (arch_rev != 0x3 && arch_rev != 0x4)
    // No GICv3 and no GICv4
    panic("ITS: Version %u is not supported.\n", arch_rev);

  // Disable ITS before trying to enable it to get an already enabled ITS into
  // quiescent state (see also the possible panic below).
  disable(base);

  _gic_cpu = gic_cpu;

  Typer typer(_its.read_non_atomic<Unsigned64>(GITS_TYPER));
  _redist_pta = typer.pta();
  _max_device_id = (1ULL << (typer.dev_bits() + 1)) - 1;
  _itt_entry_size = typer.itt_entry_size() + 1;

  Unsigned64 num_events = 1ULL << (typer.id_bits() + 1);
  if (num_lpis > num_events)
  {
    // TODO: Use per-device EventID space instead of global EventID space?
    WARN("ITS: Number of LPIs %u exceeds number of supported EventIDs %llu.\n",
         _num_lpis, num_events);
  }
  _num_lpis = num_lpis;

  Ctlr ctlr(_its.read<Unsigned32>(GITS_CTLR));
  if (ctlr.enabled() || !ctlr.quiescent())
    panic("ITS: Not in quiescent state.\n");

  init_tables(typer);
  init_cmd_queue();

  // Enable ITS
  ctlr.umsi_irq() = false;
  ctlr.enabled() = true;
  _its.write<Unsigned32>(ctlr.raw, GITS_CTLR);

  printf("ITS: %lx rev=%x num_lpis=%u num_cols=%u num_devs=%u dev_bits=%u\n",
         base, arch_rev, _num_lpis, Num_cols, Max_num_devs,
         cxx::log2u(_max_device_id + 1));
}

PRIVATE
void
Gic_its::init_tables(Typer typer)
{
  for (unsigned i = 0; i < GITS_baser_num; i++)
    {
      unsigned off = GITS_BASER + (i * 8);
      _tables[i].alloc(_its.r<Unsigned64>(off), typer);

      if (_tables[i].type() == Baser::Type_device)
        _device_table = &_tables[i];
    }

    if (!_device_table)
      panic("ITS: No device table detected.\n");
}

PRIVATE
void
Gic_its::init_cmd_queue()
{
  _cmd_queue =  Gic_mem::alloc_zmem(GITS_cmd_queue_size, GITS_cmd_queue_align);
  if (!_cmd_queue.is_valid())
    panic("ITS: Failed to allocate command queue.\n");

  Cbaser cbaser;
  cbaser.size() = (GITS_cmd_queue_size / GITS_cmd_queue_page_size) - 1;
  cbaser.pa() = _cmd_queue.phys_addr();
  cbaser.valid() = 1;
  _cmd_queue.setup_reg(_its.r<Unsigned64>(GITS_CBASER), cbaser);
  _cmd_queue.make_coherent();
  // GITS_CREADR is cleared to 0 when GITS_CBASER is written.
  _cmd_queue_write_off = 0;
  _its.write_non_atomic<Unsigned64>(_cmd_queue_write_off, GITS_CWRITER);
}

PUBLIC
void
Gic_its::cpu_init(Cpu_number cpu, Gic_redist const &redist)
{
  unsigned cpu_index = cxx::int_value<Cpu_number>(cpu);

  Collection tmp;
  if (_redist_pta)
    tmp.redist_base.phys_base_addr() = Kmem::kdir->virt_to_phys(redist.get_base());
  else
    tmp.redist_base.processor_nr() = redist.get_processor_nr();
  tmp.icid = cpu_index;
  send_cmd(Cmd::mapc(tmp.icid, tmp.redist_base, true), &tmp);
  send_cmd(Cmd::invall(tmp.icid), &tmp);

  Collection &col = _cols[cpu_index];
  col.redist_base = tmp.redist_base;
  Mem::mp_wmb();
  // Set the ICID last, as it marks the collection as valid. The collection must
  // be fully setup before it may appear valid on other CPUs.
  col.icid = tmp.icid;
}

PRIVATE inline
unsigned
Gic_its::cmd_queue_read_off()
{
  return _its.read<Mword>(GITS_CREADR) & GITS_cmd_queue_offset_mask;
}

/**
 * \pre The _cmd_queue_lock must be held.
 */
PRIVATE inline
Gic_its::Cmd *
Gic_its::alloc_cmd_slot()
{
  unsigned write_off = _cmd_queue_write_off;
  unsigned next_write_off = (write_off + Cmd::Size) % GITS_cmd_queue_size;

  L4::Poll_timeout_counter i(5000000);
  while (i.test(next_write_off == cmd_queue_read_off()))
    Proc::pause();

  if (EXPECT_FALSE(i.timed_out()))
    {
      WARNX(Error, "ITS: Command slot allocation timed out!\n");
      return nullptr;
    }

  _cmd_queue_write_off = next_write_off;
  return _cmd_queue.virt_ptr<Cmd>() + (write_off / sizeof(Cmd));
}

/**
 * \pre The _cmd_queue_lock must be held.
 */
PRIVATE inline
bool
Gic_its::enqueue_cmd(Cmd const &cmd)
{
  Cmd *slot = alloc_cmd_slot();
  if (!slot)
    return false;

  *slot = cmd;
  _cmd_queue.make_coherent(slot, slot + 1);
  return true;
}

PRIVATE inline
bool
Gic_its::is_cmd_complete(unsigned cmd_off, unsigned num_cmds)
{
    unsigned read_off = cmd_queue_read_off();
    if (read_off >= cmd_off)
      // read_off has passed cmd_off or cmd_off has wrapped around
      return read_off - cmd_off < GITS_cmd_queue_size - (num_cmds * Cmd::Size);
    else
      // read_off has not yet passed cmd_off or read_off has wrapped around
      return cmd_off - read_off > (num_cmds * Cmd::Size);
}

/**
 * Send a command to the ITS.
 *
 * \param cmd  The command to send.
 * \param col  The collection to sync the command with.
 */
PRIVATE
void
Gic_its::send_cmd(Cmd const &cmd, Collection const *col = nullptr)
{
  auto guard = lock_guard(_cmd_queue_lock);

  unsigned num_cmds = 1;
  if (EXPECT_FALSE(!enqueue_cmd(cmd)))
    return;

  if (col)
    {
      assert(col->is_valid());

      if (EXPECT_TRUE(enqueue_cmd(Cmd::sync(col->redist_base))))
        num_cmds++;
    }
  // Inform ITS about the submitted commands.
  _its.write<Mword>(_cmd_queue_write_off, GITS_CWRITER);

  unsigned wait_off = _cmd_queue_write_off;

  // We are done modifying the command queue, thus release the lock.
  guard.reset();

  // Wait for the submitted commands to complete.
  L4::Poll_timeout_counter i(5000000);
  while (i.test(!is_cmd_complete(wait_off, num_cmds)))
    Proc::pause();

  if (EXPECT_FALSE(i.timed_out()))
    WARNX(Error, "ITS: Command execution timed out!\n");
}

/**
 * \pre The _device_alloc_lock must be held.
 */
PRIVATE
Gic_its::Device *
Gic_its::get_or_alloc_device(Dev_id dev_id)
{
  for (auto &&dev : _devices)
    {
      if (dev->id() == dev_id)
        return dev;
    }

  if (_num_devs >= Max_num_devs)
    {
      WARN("ITS: Failed to allocate device %x, max device limit reached!\n",
           dev_id);
      return nullptr;
    }

  Device *device = device_alloc.new_obj(dev_id, *this);
  if (!device)
    {
      WARN("ITS: Failed to allocate device %x!\n", dev_id);
      return nullptr;
    }

  if (!device->setup_itt())
    {
      WARN("ITS: Failed to setup ITT for device %x!\n", dev_id);
      device_alloc.del(device);
      return nullptr;
    }

  _devices.add(device);
  _num_devs++;
  return device;
}

/**
 * \pre The _device_alloc_lock must be held.
 * \pre The lpi.lock must be held.
 */
PRIVATE
void
Gic_its::unbind_lpi_from_device(Lpi &lpi)
{
  if (Device *device = lpi.device)
    {
      device->unbind_lpi(lpi);

      if (!device->has_lpis())
        {
          // Device is unused now, delete it.
          device_alloc.del(device);
          _num_devs--;
        }
    }
}

/**
 * \pre The lpi.lock must be held.
 */
PUBLIC
int
Gic_its::bind_lpi_to_device(Lpi &lpi, Unsigned32 src, Irq_mgr::Msi_info *inf)
{
  if (src > _max_device_id)
    {
      WARN("ITS: 0x%x is not a valid DeviceID!\n", src);
      return -L4_err::ERange;
    }

  if (!lpi.device || lpi.device->id() != src)
    {
      auto guard = lock_guard(_device_alloc_lock);

      unbind_lpi_from_device(lpi);

      auto device = get_or_alloc_device(src);
      if (!device)
        return -L4_err::ENomem;

      device->bind_lpi(lpi);
    }

  inf->data = lpi.event_id();
  // TODO: Must be mapped in the DMA space of the device if IOMMU is enabled.
  inf->addr = Kmem::kdir->virt_to_phys(_its.get_mmio_base()) + GITS_TRANSLATER;
  return 0;
}

/**
 * \pre The lpi.lock must be held.
 */
PUBLIC
void
Gic_its::free_lpi(Lpi &lpi)
{
  auto guard = lock_guard(_device_alloc_lock);
  unbind_lpi_from_device(lpi);
  lpi.reset();
}

/**
 * \pre The lpi.lock must be held.
 */
PUBLIC inline
void
Gic_its::ack_lpi(Lpi &lpi)
{
  // TODO: Acknowledging an LPI could and probably should be optimized by
  // moving it out of Gic_its into Gic_msi. Gic_msi would not even have to
  // lookup the Lpi, but instead could directly invoke
  // _gic_cpu->ack(Gic_dist::Lpi_intid_base + pin), see the implementation of
  // Lpi::intid().
  _gic_cpu->ack(lpi.intid());
}

/**
 * \pre The lpi.lock must be held.
 */
PRIVATE inline
void
Gic_its::update_lpi_config(Lpi &lpi, bool enable)
{
  Gic_redist::enable_lpi(lpi.intid() - Gic_dist::Lpi_intid_base, enable);

  if (lpi.device && lpi.col)
  {
    send_cmd(Cmd::inv(lpi.device->id(), lpi.event_id()), lpi.col);
  }
}

PUBLIC inline NEEDS [Gic_its::update_lpi_config]
/**
 * \pre The lpi.lock must be held.
 */
void
Gic_its::mask_lpi(Lpi &lpi)
{
  update_lpi_config(lpi, false);
}

/**
 * \pre The lpi.lock must be held.
 */
PUBLIC inline NEEDS [Gic_its::update_lpi_config]
void
Gic_its::unmask_lpi(Lpi &lpi)
{
  update_lpi_config(lpi, true);
}

/**
 * \pre The lpi.lock must be held.
 */
PUBLIC
void
Gic_its::assign_lpi_to_cpu(Lpi &lpi, Cpu_number cpu)
{
  Collection const *col = get_col(cpu);
  if (EXPECT_FALSE(!col->is_valid()))
    {
      WARN("ITS: Tried to assign LPI %u to uninitialized CPU %u.\n",
           lpi.intid(), cxx::int_value<Cpu_number>(cpu));
      return;
    }

  if (lpi.col != col)
    {
      if (lpi.device && lpi.col)
        {
          send_cmd(Cmd::movi(lpi.device->id(), lpi.event_id(), col->icid), col);
        }
      else if (lpi.device)
        {
          send_cmd(Cmd::mapti(lpi.device->id(), lpi.event_id(), lpi.intid(),
                              col->icid), col);
        }

      lpi.col = col;
    }
}

PRIVATE inline
Gic_its::Collection const *
Gic_its::get_col(Cpu_number cpu) const
{
  return &_cols[cxx::int_value<Cpu_number>(cpu)];
}

PUBLIC inline
unsigned
Gic_its::num_lpis() const
{
  return _num_lpis;
}

/**
 * \pre The _device_alloc_lock must be held.
 */
IMPLEMENT
bool
Gic_its::Device::setup_itt()
{
  assert(!_itt.is_valid());

  _itt = Gic_mem::alloc_zmem(itt_size(), 256);
  if (!_itt.is_valid())
    return false;

  // Allocate second-level device table if necessary.
  if (!_its._device_table->ensure_id_present(_id))
    {
      _itt.free();
      return false;
    }
  _itt.inherit_mem_attribs(_its._device_table->mem());

  _itt.make_coherent();
  _its.send_cmd(Cmd::mapd(_id, _itt.phys_addr(), cxx::log2u(_its.num_lpis())));
  return true;
}

/**
 * \pre The _device_alloc_lock must be held.
 */
IMPLEMENT
void
Gic_its::Device::free_itt()
{
  assert(_lpi_count == 0);
  assert(_itt.is_valid());

  _its.send_cmd(Cmd::mapd(_id, 0, 0));
  _itt.free();
}

IMPLEMENT
unsigned
Gic_its::Device::itt_size()
{
  return _its.num_lpis() * _its._itt_entry_size;
}

/**
 * \pre The _device_alloc_lock must be held.
 * \pre The lpi.lock must be held.
 */
IMPLEMENT
void
Gic_its::Device::bind_lpi(Lpi &lpi)
{
  assert(lpi.device == nullptr);

  lpi.device = this;
  _lpi_count++;

  if (lpi.col)
    _its.send_cmd(Cmd::mapti(_id, lpi.event_id(), lpi.intid(), lpi.col->icid),
                  lpi.col);
}

/**
 * \pre The _device_alloc_lock must be held.
 * \pre The lpi.lock must be held.
 */
IMPLEMENT
void
Gic_its::Device::unbind_lpi(Lpi &lpi)
{
  assert(lpi.device == this);

  if (lpi.col)
    _its.send_cmd(Cmd::discard(_id, lpi.event_id()), lpi.col);

  lpi.device = nullptr;
  _lpi_count--;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [have_arm_gic_msi]:

#include "mmio_register_block.h"
#include "panic.h"
#include "poll_timeout_counter.h"
#include "processor.h"
#include <cstdio>

/**
 * Disable the ITS to prevent triggering of unexpected LPIs.
 */
PUBLIC static
void
Gic_its::disable(Address base)
{
  auto its = Mmio_register_block(base);

  unsigned arch_rev = (its.read<Unsigned32>(GITS_PIDR2) >> 4) & 0xf;
  if (arch_rev != 0x3 && arch_rev != 0x4)
    // No GICv3 and no GICv4 -- we cannot disable ITS!
    panic("ITS: Version %u is not supported.\n", arch_rev);

  Ctlr ctlr(its.read<Unsigned32>(GITS_CTLR));
  if (!ctlr.enabled() && ctlr.quiescent())
    return;

  ctlr.umsi_irq() = false;
  ctlr.enabled() = false;
  its.write<Unsigned32>(ctlr.raw, GITS_CTLR);

  // Wait for quiescent state
  L4::Poll_timeout_counter i(5000000);
  while (i.test(!Ctlr(its.read<Unsigned32>(GITS_CTLR)).quiescent()))
    Proc::pause();
  if (Ctlr(its.read<Unsigned32>(GITS_CTLR)).quiescent())
    printf("ITS: Disabled.\n");
  else
    panic("ITS: Trying to disable: Not in quiescent state!\n");
}
