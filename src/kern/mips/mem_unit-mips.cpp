INTERFACE:

#include "types.h"
#include "processor.h"

class Mem_unit
{
public:
  enum : unsigned short
  {
    Asid_invalid = 0xffff,
    Asid_mask    = 0x3ff,
  };

  enum : Mword
  {
    Entry_hi_EHINV = 1U << 10
  };

  static void index_reg(Signed32 v)
  { Mips::mtc0_32(v, Mips::Cp0_index); }

  static Signed32 index_reg()
  { return Mips::mfc0_32(Mips::Cp0_index); }

  static void entry_lo0(Mword v)
  { Mips::mtc0(v, Mips::Cp0_entry_lo1); }

  static void entry_lo1(Mword v)
  { Mips::mtc0(v, Mips::Cp0_entry_lo2); }

  static Mword entry_lo0()
  { return Mips::mfc0(Mips::Cp0_entry_lo1); }

  static Mword entry_lo1()
  { return Mips::mfc0(Mips::Cp0_entry_lo2); }

  static void page_mask(Mword v)
  { Mips::mtc0_32(v, Mips::Cp0_page_mask); }

  static void page_grain(Unsigned32 v)
  { Mips::mtc0_32(v, Mips::Cp0_page_grain); }

  static void pw_base(Mword v)
  { Mips::mtc0(v, Mips::Cp0_pw_base); }

  static void pw_field(Mword v)
  { Mips::mtc0(v, Mips::Cp0_pw_field); }

  static void pw_size(Mword v)
  { Mips::mtc0(v, Mips::Cp0_pw_size); }

  static void wired(Unsigned32 v)
  { Mips::mtc0_32(v, Mips::Cp0_wired); }

  static Unsigned32 wired()
  { return Mips::mfc0_32(Mips::Cp0_wired); }

  static void pw_ctl(Unsigned32 v)
  { Mips::mtc0_32(v, Mips::Cp0_pw_ctl); }

  static Unsigned32 pw_ctl()
  { return Mips::mfc0_32(Mips::Cp0_pw_ctl); }

  static void entry_hi(Mword v)
  { Mips::mtc0(v, Mips::Cp0_entry_hi); }

  static Mword entry_hi()
  { return Mips::mfc0(Mips::Cp0_entry_hi); }

  static void make_coherent_to_pou(void const *addr)
  { Mips::synci(addr); }

private:
  static void (*_tlb_flush)(long asid, unsigned guest_id);
  static void (*_tlb_flush_full)();
  static void (*_vz_guest_tlb_flush)(unsigned guest_id);
};

IMPLEMENTATION:

#include "cpu.h"
#include "cpu_lock.h"
#include "lock_guard.h"

/**
 * TLB flush callback for ASID / Guest ID based flushes.
 */
void (*Mem_unit::_tlb_flush)(long asid, unsigned guest_id);
/**
 * TLB flush callback for full flush (only Guest ID 0).
 */
void (*Mem_unit::_tlb_flush_full)();

void (*Mem_unit::_vz_guest_tlb_flush)(unsigned guest_id);

PUBLIC static inline
void
Mem_unit::tlb_flush(long asid, unsigned guest_id)
{ _tlb_flush(asid, guest_id); }

PUBLIC static inline
void
Mem_unit::tlb_flush()
{ _tlb_flush_full(); }

PUBLIC static inline
void
Mem_unit::vz_guest_tlb_flush(unsigned guest_id)
{ _vz_guest_tlb_flush(guest_id); }

static unsigned _l1_i_line;
static unsigned _l1_d_line;
static unsigned _l2_line;
static unsigned _l3_line;

PUBLIC static
void
Mem_unit::cache_detect()
{
  auto c1 = Mips::Cfg<1>::read();
  if (c1.il())
    _l1_i_line = 2U << c1.il();

  if (c1.dl())
    _l1_d_line = 2U << c1.dl();

  if (!c1.m())
    return;

  auto c2 = Mips::Cfg<2>::read();
  if (c2.sl())
    _l2_line = 2U << c2.sl();

  if (c2.tl())
    _l3_line = 2U << c2.tl();
}

PUBLIC static
void
Mem_unit::dcache_flush(Address start, Address end)
{
  if (_l1_d_line)
    {
      for (Address x = start; x < end; x += _l1_d_line)
        asm ("cache 0x15, (%0)" : : "r"(x));

      asm volatile ("sync");

      if (_l2_line)
        {
          for (Address x = start; x < end; x += _l2_line)
            asm ("cache 0x17, (%0)" : : "r"(x));

          asm volatile ("sync");
        }

      if (_l3_line)
        {
          for (Address x = start; x < end; x += _l3_line)
            asm ("cache 0x16, (%0)" : : "r"(x));

          asm volatile ("sync");
        }
    }
}

PUBLIC static
void
Mem_unit::dcache_inv(Address start, Address end)
{
  if (_l1_d_line)
    {
      for (Address x = start; x < end; x += _l1_d_line)
        asm ("cache 0x11, (%0)" : : "r"(x));

      asm volatile ("sync");

      if (_l2_line)
        {
          for (Address x = start; x < end; x += _l2_line)
            asm ("cache 0x13, (%0)" : : "r"(x));

          asm volatile ("sync");
        }

      if (_l3_line)
        {
          for (Address x = start; x < end; x += _l3_line)
            asm ("cache 0x12, (%0)" : : "r"(x));

          asm volatile ("sync");
        }
    }
}

PUBLIC static
void
Mem_unit::dcache_clean(Address start, Address end)
{
  if (_l1_d_line)
    {
      for (Address x = start; x < end; x += _l1_d_line)
        asm ("cache 0x19, (%0)" : : "r"(x));

      asm volatile ("sync");

      if (_l2_line)
        {
          for (Address x = start; x < end; x += _l2_line)
            asm ("cache 0x1b, (%0)" : : "r"(x));

          asm volatile ("sync");
        }

      if (_l3_line)
        {
          for (Address x = start; x < end; x += _l3_line)
            asm ("cache 0x1a, (%0)" : : "r"(x));

          asm volatile ("sync");
        }
    }
}

PUBLIC static
Signed32
Mem_unit::tlb_probe()
{
  __asm__ __volatile__ (
      "tlbp             \n"
      "ehb              \n"
  );
  return index_reg();
}

/**
 * Write the (root) TLB entry at the current `Index` and `GuestID.RID`.
 * \param v_entry_hi   Value for `EntryHi`.
 * \param v_entry_lo0  Value for `EntryLo0`.
 * \param v_entry_lo1  Value for `EntryLo1`.
 * \param v_page_mask  Value for `PageMask`.
 *
 * \pre CP0 `Index` register must be loaded with the index that shall be
 *      written.
 * \pre If other CP0 registers such as `GuestCtl1` are needed for`TLBWI`
 *      those must be setup before calling this function.
 */
PRIVATE static inline NOEXPORT
void
Mem_unit::tlb_write(Mword v_entry_hi, Mword v_entry_lo0,
                    Mword v_entry_lo1, Mword v_page_mask)
{
  entry_hi(v_entry_hi);
  entry_lo0(v_entry_lo0);
  entry_lo1(v_entry_lo1);
  page_mask(v_page_mask);
  Mips::ehb();
  Mips::tlbwi();
}

/**
 * Read (root) TLB entry at the given `index` and return `EntryHi`.
 * \param  index  The index of the TLB entry that shall be red.
 * \return Value of CP0 `EntryHi` register.
 * \pre `index` must be a valid index.
 * \pre The `TLBR` instruction must be supported.
 */
PRIVATE static inline NOEXPORT
Mword
Mem_unit::tlb_read(Unsigned32 index)
{
  index_reg(index);
  Mips::ehb();
  Mips::tlbr();
  Mips::ehb();
  return entry_hi();
}

/**
 * JTLB flush for given ASID for non-VZ CPUs without TLBINV instruction.
 *
 * \param asid      The address space ID that shall be completely flushed from
 *                  the TLB. Values less than zero are unassigned ASIDs and the
 *                  flush shall be skipped.
 * \param guest_id  VZ guest ID (unused by this implementation).
 *
 * \pre Not reentrant. Must execute with disabled preemption.
 *
 * This function uses unique entries with virtual addresses in `kseg1` and with
 * zero EntryLo0 and EntryLo1 values to overwrite matching TLB entries.
 *
 * \note If immediate visibility for TLB changes is needed an additional `ehb`
 *       is requred.
 *
 * \remark This implementation might overwrite wired entries.
 * \remark Assumes no global entries in the TLB.
 */
PRIVATE static
void
Mem_unit::_plain_tlb_flush(long asid, unsigned guest_id)
{
  (void)guest_id;
  if (EXPECT_FALSE(asid < 0))
    return;

  Mword saved_hi = entry_hi();
  for (int idx = Cpu::tlb_size() - 1; idx >= 0; --idx)
    {
      Mword e = tlb_read(idx);

      if (is_unique_hi(e) || (e & Asid_mask) != (unsigned long)asid)
        continue;

      auto lo = entry_lo0() | entry_lo1();
      // at least one EntryLo is valid
      if ((lo & 2) == 0)
        continue;

      tlb_write(unique_hi(idx, asid), 0, 0, 0);
    }
  entry_hi(saved_hi);
}

/**
 * JTLB full flush for non-VZ CPUs without TLBINV instruction.
 */
PRIVATE static
void
Mem_unit::_plain_tlb_flush_full()
{
  Mword saved_hi = entry_hi();
  for (int idx = Cpu::tlb_size() - 1; idx >= 0; --idx)
    {
      index_reg(idx);
      tlb_write(unique_hi(idx, 0), 0, 0, 0);
    }
  entry_hi(saved_hi);
}

PUBLIC static inline
Mword
Mem_unit::vz_guest_ctl1()
{ return Mips::mfc0_32(Mips::Cp0_guest_ctl_1); }

PRIVATE static inline
void
Mem_unit::set_vz_guest_ctl1(Mword ctl1)
{ Mips::mtc0_32(ctl1, Mips::Cp0_guest_ctl_1); }

PUBLIC static inline NEEDS[Mem_unit::set_vz_guest_ctl1]
void
Mem_unit::set_vz_guest_rid(Mword ctl1_orig, Mword guest_id)
{
  set_vz_guest_ctl1((ctl1_orig & ~0x00ff0000UL) | ((guest_id & 0x00ff) << 16));
}

/**
 * JTLB flush for given ASID / Guest ID for VZ CPUs without TLBINV instruction.
 *
 * \param asid      The address space ID that shall be completely flushed from
 *                  the TLB. Values less than zero are unassigned ASIDs and the
 *                  flush shall be skipped.
 * \param guest_id  VZ guest ID to be flushed. Value `0` means no guest entries
 *                  shall mbe flushed (only root mappings with given ASID are
 *                  affected.
 *
 * \pre Not reentrant. Must execute with disabled preemption.
 *
 * This function invalidates all TLB entries that either have guest ID `0` and
 * match the given `asid`, or if `guest_id` is not zero match the given guest
 * ID. The `EntryHi.EHINV` flag is used to detect invalid TLB entries and
 * to explicitly invalidate entries.
 *
 * \note If immediate visibility for TLB changes is needed an additional `ehb`
 *       is requred.
 *
 * \remark This implementation might overwrite wired root entries.
 * \remark Assumes support for `EntryHi.EHINV`.
 * \remark Assumes no global entries in the TLB.
 * \remark Assumes that in a shared root / guest TLB config TLB indices
 *         containing guest TLB entries return `EntryHi.EHINV = 1`. Otherwise
 *         it might also affect guest wired entries (could break guest OS).
 */
PRIVATE static
void
Mem_unit::_vz_tlb_flush(long asid, unsigned guest_id)
{
  if (EXPECT_FALSE((asid < 0) && (guest_id == 0)))
    return;

  // NOTE: we assume a JTLB
  Mword saved_hi = entry_hi();
  for (int idx = Cpu::tlb_size() - 1; idx >= 0; --idx)
    {
      Mword e = tlb_read(idx);

      // We assume that a CPU with VZ and GuestID support has EHINV support
      if (e & Entry_hi_EHINV)
        continue;

      Mword ctl1 = vz_guest_ctl1();
      auto egid = (ctl1 >> 16) & 0xff;
      if (   (egid && egid == guest_id)
          || (egid == 0 && asid >= 0 && (e & Asid_mask) == (unsigned long)asid))
        {
          entry_hi(Entry_hi_EHINV);
          Mips::ehb();
          Mips::tlbwi();
        }
    }
  entry_hi(saved_hi);
}

/**
 * Flush Guest TLB for given guest ID without tlbinv support.
 *
 * \remark Assumes EntryHi.EHINV support.
 */
PRIVATE static
void
Mem_unit::_vz_guest_tlb_flush_impl(unsigned guest_id)
{
  // Assume that no guest CP0 TLB/MMU context is life and will be reloaded
  // later anyways
  for (int idx = Cpu::tlb_size() - 1; idx >= 0; --idx)
    {
      // Guest:index = idx
      Mips::mtgc0_32(idx, Mips::Cp0_index);
      Mips::ehb();
      asm volatile (".set push; .set virt; tlbgr; .set pop");
      Mips::ehb();
      Mword ctl1 = vz_guest_ctl1();
      auto egid = (ctl1 >> 16) & 0xff;
      if (egid != guest_id)
        continue;

      Mword e = Mips::mfgc0(Mips::Cp0_entry_hi);

      // We assume thet c CPU with VZ and GuestID support has EHINV support
      if (e & Entry_hi_EHINV)
        continue;

      Mips::mtgc0(Entry_hi_EHINV, Mips::Cp0_entry_hi);
      asm volatile (
          ".set push; .set virt\n\t"
          "ehb\n\t"
          "tlbgwi\n\t"
          ".set pop");
    }
}

/**
 * JTLB full flush for VZ CPUs without TLBINV instruction.
 *
 * \pre Not reentrant. Must execute with disabled preemption.
 *
 * ID. The `EntryHi.EHINV` flag is used to detect invalid TLB entries and
 * to explicitely invalidate entries.
 *
 * \note If immediate visibility for TLB changes is needed an additional `ehb`
 *       is requred.
 *
 * \remark This implementation might overwrite wired root entries.
 * \remark Assumes support for `EntryHi.EHINV`.
 * \remark Assumes no global entries in the TLB.
 * \remark Assumes that in a shared root / guest TLB config TLB indices
 *         containing guest TLB entries return `EntryHi.EHINV = 1`. Otherwise
 *         it might also affect guest wired entries (could break guest OS).
 */
PRIVATE static
void
Mem_unit::_vz_tlb_flush_full()
{
  using namespace Mips;
  Mword saved_hi = entry_hi();
  for (int idx = Cpu::tlb_size() - 1; idx >= 0; --idx)
    {
      Mword e = tlb_read(idx);

      // We assume thet c CPU with VZ and GuestID support has EHINV support
      if (e & Entry_hi_EHINV)
        continue;

      entry_hi(Entry_hi_EHINV);
      ehb();
      tlbwi();
    }
  entry_hi(saved_hi);
}

/**
 * JTLB flush for given ASID / Guest ID for VZ CPUs with TLBINV instruction.
 *
 * \param asid      The address space ID that shall be completely flushed from
 *                  the TLB. Values less than zero are unassigned ASIDs and the
 *                  flush shall be skipped.
 * \param guest_id  VZ guest ID to be flushed. Value `0` means no guest entries
 *                  shall be flushed (only root mappings with given ASID are
 *                  affected).
 *
 * \pre Not reentrant. Must execute with disabled preemption.
 *
 * This function invalidates all TLB entries that either have guest ID `0` and
 * match the given `asid`, or if `guest_id` is not zero match the given guest
 * ID.
 *
 * \note If immediate visibility for TLB changes is needed an additional `ehb`
 *       is requred.
 *
 * \remark This implementation might overwrite wired root entries.
 * \remark Assumes that TLBINV and TLBINVF do not affect guest TLB entries
 *         in a shared root / guest TLB setup. Otherwise wired guest entries
 *         could be affected.
 */
PRIVATE static
void
Mem_unit::_vz_tlbinv_tlb_flush(long asid, unsigned guest_id)
{
  using namespace Mips;
  Mword ctl1 = vz_guest_ctl1();

  if (asid >= 0)
    {
      set_vz_guest_rid(ctl1, 0);
      Mword saved_hi = entry_hi();
      entry_hi(asid);
      ehb();
      tlbinv();
      entry_hi(saved_hi);
    }

  if (guest_id != 0)
    {
      set_vz_guest_rid(ctl1, guest_id);
      ehb();
      tlbinvf();
    }
}

/**
 * Flush Guest TLB for given guest ID with tlbinv support.
 */
PRIVATE static
void
Mem_unit::_vz_guest_tlbinv_tlb_flush_impl(unsigned guest_id)
{
  set_vz_guest_rid(vz_guest_ctl1(), guest_id);
  Mips::ehb();
  asm volatile (".set push; .set virt; tlbginvf; .set pop");
}

/**
 * JTLB full flush for VZ CPUs with GuestID and TLBINV instructions.
 *
 * \note only the root TLB and guest ID are affected.
 */
PRIVATE static
void
Mem_unit::_vz_tlbinv_tlb_flush_full()
{
  set_vz_guest_rid(vz_guest_ctl1(), 0);
  Mips::ehb();
  Mips::tlbinvf();
}

/**
 * VTLB/FTLB flush for given ASID / Guest ID for VZ CPUs with TLBINV instruction.
 *
 * \param asid      The address space ID that shall be completely flushed from
 *                  the TLB. Values less than zero are unassigned ASIDs and the
 *                  flush shall be skipped.
 * \param guest_id  VZ guest ID to be flushed. Value `0` means no guest entries
 *                  shall mbe flushed (only root mappings with given ASID are
 *                  affected.
 *
 * \pre Not reentrant. Must execute with disabled preemption.
 *
 * This function invalidates all TLB entries that either have guest ID `0` and
 * match the given `asid`, or if `guest_id` is not zero match the given guest
 * ID.
 *
 * \note If immediate visibility for TLB changes is needed an additional `ehb`
 *       is requred.
 *
 * \remark This implementation might overwrite wired root entries.
 * \remark Assumes that TLBINV and TLBINVF do not affect guest TLB entries
 *         in a shared root / guest TLB setup. Otherwise wired guest entries
 *         could be affected.
 */
PRIVATE static
void
Mem_unit::_vz_tlbinv_ftlb_flush_loop(long asid, unsigned guest_id)
{
  Mword ctl1 = vz_guest_ctl1();

  if (asid >= 0)
    {
      Mword saved_hi = entry_hi();
      index_reg(0); // VTLB index
      set_vz_guest_rid(ctl1, 0);
      entry_hi(asid);
      Mips::ehb();
      Mips::tlbinv();
      for (unsigned i = Cpu::tlb_size(); i < Cpu::tlb_size() + Cpu::ftlb_sets(); ++i)
        {
          index_reg(i);
          Mips::ehb();
          Mips::tlbinv();
        }
      entry_hi(saved_hi);
    }

  if (guest_id != 0)
    {
      set_vz_guest_rid(ctl1, guest_id);
      Mips::ehb();
      Mips::tlbinvf();
      for (unsigned i = Cpu::tlb_size(); i < Cpu::tlb_size() + Cpu::ftlb_sets(); ++i)
        {
          index_reg(i);
          Mips::ehb();
          Mips::tlbinvf();
        }
    }
}

/**
 * VTLB/FTLB full flush for VZ CPUs with GuestID and TLBINV instructions.
 *
 * \note only the root TLB and guest ID are affected.
 */
PRIVATE static
void
Mem_unit::_vz_tlbinv_ftlb_flush_loop_full()
{
  set_vz_guest_rid(vz_guest_ctl1(), 0);
  Mips::ehb();
  Mips::tlbinvf();
  for (unsigned i = Cpu::tlb_size(); i < Cpu::tlb_size() + Cpu::ftlb_sets(); ++i)
    {
      index_reg(i);
      Mips::ehb();
      Mips::tlbinvf();
    }
}

/**
 * VTLB/FTLB flush for given ASID / Guest ID for non-VZ CPUs with TLBINV instruction.
 *
 * \param asid      The address space ID that shall be completely flushed from
 *                  the TLB. Values less than zero are unassigned ASIDs and the
 *                  flush shall be skipped.
 * \param guest_id  VZ guest ID to be flushed (unused).
 *
 * \pre Not reentrant. Must execute with disabled preemption.
 *
 * This function invalidates all TLB entries that match the given `asid`.
 *
 * \note If immediate visibility for TLB changes is needed an additional `ehb`
 *       is requred.
 *
 * \remark This implementation might overwrite wired root entries.
 */
PRIVATE static
void
Mem_unit::_tlbinv_ftlb_flush_loop(long asid, unsigned guest_id)
{
  (void) guest_id;
  if (asid < 0)
    return;

  Mword saved_hi = entry_hi();
  index_reg(0); // VTLB index
  entry_hi(asid);
  Mips::ehb();
  Mips::tlbinv();

  for (unsigned i = Cpu::tlb_size(); i < Cpu::tlb_size() + Cpu::ftlb_sets(); ++i)
    {
      index_reg(i);
      Mips::ehb();
      Mips::tlbinv();
    }

  entry_hi(saved_hi);
}

/**
 * VTLB/FTLB full flush for non-VZ CPUs with TLBINV instructions.
 */
PRIVATE static
void
Mem_unit::_tlbinv_ftlb_flush_loop_full()
{
  index_reg(0);
  Mips::ehb();
  Mips::tlbinvf();
  for (unsigned i = Cpu::tlb_size(); i < Cpu::tlb_size() + Cpu::ftlb_sets(); ++i)
    {
      index_reg(i);
      Mips::ehb();
      Mips::tlbinvf();
    }
}

/**
 * JTLB flush for given ASID for non-VZ CPUs with TLBINV instructions.
 *
 * \param asid      The address space ID that shall be completely flushed from
 *                  the TLB. Values less than zero are unassigned ASIDs and the
 *                  flush shall be skipped.
 * \param guest_id  VZ guest ID to be flushed (unused).
 *
 * \pre Not reentrant. Must execute with disabled preemption.
 *
 * This function invalidates all TLB entries that match the given `asid`.
 *
 * \note If immediate visibility for TLB changes is needed an additional `ehb`
 *       is requred.
 *
 * \remark This implementation might overwrite wired root entries.
 */
PRIVATE static
void
Mem_unit::_tlbinv_tlb_flush(long asid, unsigned guest_id)
{
  (void) guest_id;
  if (EXPECT_FALSE(asid < 0))
    return;

  Mword saved_hi = entry_hi();
  entry_hi(asid);
  Mips::ehb();
  Mips::tlbinv();
  entry_hi(saved_hi);
}

/**
 * JTLB full flush for non-VZ CPUs with TLBINV instructions.
 */
PRIVATE static
void
Mem_unit::_tlbinv_tlb_flush_full()
{
  Mips::tlbinvf();
}

PUBLIC static
void
Mem_unit::init_tlb()
{
  if (Cpu::options.vz() && Cpu::options.tlbinv())
    {
      if (Cpu::options.ftlb() && !Cpu::options.ftlbinv())
        {
          _tlb_flush = &_vz_tlbinv_ftlb_flush_loop;
          _tlb_flush_full = &_vz_tlbinv_ftlb_flush_loop_full;
        }
      else
        {
          _tlb_flush = &_vz_tlbinv_tlb_flush;
          _tlb_flush_full = &_vz_tlbinv_tlb_flush_full;
        }
      _vz_guest_tlb_flush = &_vz_guest_tlbinv_tlb_flush_impl;
    }
  else if (Cpu::options.vz())
    {
      _tlb_flush = &_vz_tlb_flush;
      _tlb_flush_full = &_vz_tlb_flush_full;
      _vz_guest_tlb_flush = &_vz_guest_tlb_flush_impl;
    }
  else if (Cpu::options.tlbinv())
    {
      if (Cpu::options.ftlb() && !Cpu::options.ftlbinv())
        {
          _tlb_flush = &_tlbinv_ftlb_flush_loop;
          _tlb_flush_full = &_tlbinv_ftlb_flush_loop_full;
        }
      else
        {
          _tlb_flush = &_tlbinv_tlb_flush;
          _tlb_flush_full = &_tlbinv_tlb_flush_full;
        }
    }
  else
    {
      _tlb_flush = &_plain_tlb_flush;
      _tlb_flush_full = &_plain_tlb_flush_full;
    }

  wired(0);
  if (Cpu::options.tlbinv())
    Mips::tlbinvf();
  else
    {
      Address va = 0x80000000; // kseg0
      for (unsigned e = 0; e < Cpu::tlb_size(); ++e)
        {
          for (;;)
            {
              entry_hi(va);
              Mips::ehb();
              if (tlb_probe() < 0)
                break;

              // rewrite the TLB entry to the smallest page size to avoid
              // cascading overlaps
              tlb_write(va, 0, 0, 0);
              va += 1UL << 13; // use 8K steps as minimum page size
            }

          index_reg(e);
          tlb_write(va, 0, 0, 0);
        }
    }
}

PUBLIC static inline
void Mem_unit::set_current_asid(unsigned long asid)
{ entry_hi(asid); }


/**
 * Create a unique VA for the given TLB index and asid.
 *
 * We use 0xa0000000 for that bacause 0x80000000 is used
 * in the TLB init and we want to be sure that we do never collide with
 * these entries.
 */
PUBLIC static inline
Mword
Mem_unit::unique_hi(Mword idx, Mword asid)
{
  return (idx << 13) | 0xa0000000 | asid;
}

PUBLIC static inline
bool
Mem_unit::is_unique_hi(Mword entry_hi)
{
  entry_hi >>= 13;
  return    entry_hi >= (0xa0000000 >> 13)
         && entry_hi < (0xc0000000 >> 13);
}

