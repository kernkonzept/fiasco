INTERFACE:

#include <cxx/static_vector>

class Mem_desc
{
public:
  enum Mem_type
  {
    Undefined    = 0x0,
    Conventional = 0x1,
    Reserved     = 0x2,
    Dedicated    = 0x3,
    Shared       = 0x4,
    Kernel_tmp   = 0x7,

    Info         = 0xd,
    Bootloader   = 0xe,
    Arch         = 0xf,
  };

  enum Ext_type_info
  {
    Info_acpi_rsdp = 0
  };

private:
  Mword _l, _h;
};

//----------------------------------------------------------------------------
INTERFACE:

#include "types.h"
#include "global_data.h"

class Kip
{
public:
  enum : Cpu_time
  {
    Clock_1_second = 1000000ULL,
    Clock_1_hour   = 3600ULL * Clock_1_second,
    Clock_1_day    = 24ULL * Clock_1_hour,
    Clock_1_year   = 365UL * Clock_1_day,
  };

  void print() const;

  char const *version_string() const;

  Cpu_time clock() const;
  void set_clock(Cpu_time c);
  void add_to_clock(Cpu_time plus);

  /* 0x00 */
  Unsigned32 magic;
  Unsigned32 version;
  Unsigned8  offset_version_strings;
  Unsigned8  _fill0[3];
  Unsigned8  kip_sys_calls;
  Unsigned8  node;
  Unsigned8  _fill1[2];

  /* 0x10 */
  Unsigned64            sigma0_ip;           ///< Sigma0 instruction pointer
  Unsigned64            root_ip;             ///< Root task instruction pointer
  /* 0x20 */
  volatile Cpu_time     _clock; // don't access directly, use clock() instead!
                                // not updated in certain configurations
  Unsigned64            frequency_cpu;       ///< CPU frequency in kHz
  /* 0x30 */
  Unsigned64            acpi_rsdp_addr;      ///< ACPI RSDP/XSDP
  Unsigned64            dt_addr;             ///< Device Tree
  /* 0x40 */
  Unsigned64            user_ptr;            ///< l4-mod-info pointer
  Unsigned64            mbt_counter;         // only used for model-based testing
  /* 0x50 */
  Unsigned32            sched_granularity;   ///< for rounding time slices
  Unsigned32            _mem_descs;          ///< memory descriptors relative to Kip
  Unsigned32            _mem_descs_num;      ///< number of memory descriptors
  Unsigned32            _res1[1];            ///< \internal

  /* 0x60: */
  Unsigned64            _res2[2];            ///< \internal - spare space

  /* 0x70:
   * Platform_info. */

  /* 0x70 + sizeof(Platform_info):
   * - Memory descriptors (2 Mwords per descriptor),
   * - kernel version string ('\0'-terminated),
   * - feature strings ('\0'-terminated)
   * - terminating '\0'. */

  /* 0x800-0x900:
   * Code for syscall invocation. */

  /* 0x900-0x97F:
   * Code for reading the system clock with microsecond resolution. Depending on
   * the configuration, this clock might or might not be synchronized with the
   * KIP clock. */

  /* 0x980-0x9FF:
   * Code for reading the system clock with nanosecond resolution. Depending on
   * the configuration, this clock might or might not be synchronized with the
   * KIP clock. */

private:
  static Global_data<Kip *> global_kip;
};

#define L4_KERNEL_INFO_MAGIC (0x4BE6344CL) /* "L4µK" */

//============================================================================
IMPLEMENTATION:

#include "assert.h"
#include "config.h"
#include "mem.h"
#include "panic.h"
#include "version.h"

PUBLIC inline
Mem_desc::Mem_desc(Address start, Address end, Mem_type type, bool virt = false,
                   unsigned sub_type = 0)
: _l((start & ~0x3ffUL) | (type & 0x0f) | ((sub_type << 4) & 0x0f0)
     | (virt ? 0x0200 : 0x0)),
  _h(end)
{}

PUBLIC inline ALWAYS_INLINE
Address Mem_desc::start() const
{ return _l & ~0x3ffUL; }

PUBLIC inline ALWAYS_INLINE
Address Mem_desc::end() const
{ return _h | 0x3ffUL; }

PUBLIC inline ALWAYS_INLINE
Address Mem_desc::size() const
{ return end() - start() + 1; }

PUBLIC inline ALWAYS_INLINE
void
Mem_desc::type(Mem_type t)
{ _l = (_l & ~0x0f) | (t & 0x0f); }

PUBLIC inline ALWAYS_INLINE
Mem_desc::Mem_type Mem_desc::type() const
{ return static_cast<Mem_type>(_l & 0x0f); }

PUBLIC inline
void
Mem_desc::ext_type(unsigned t)
{ _l = (_l & ~0xf0) | ((t << 4) & 0xf0); }

PUBLIC inline
unsigned Mem_desc::ext_type() const
{ return (_l >> 4) & 0x0f; }

PUBLIC inline ALWAYS_INLINE
unsigned Mem_desc::is_virtual() const
{ return _l & 0x200; }

PUBLIC inline ALWAYS_INLINE
unsigned Mem_desc::eager_map() const
{ return _l & 0x100; }

PUBLIC inline
bool Mem_desc::contains(Address addr) const
{
  return start() <= addr && end() >= addr;
}

PUBLIC inline ALWAYS_INLINE
bool Mem_desc::valid() const
{ return type() && start() < end(); }

PRIVATE inline ALWAYS_INLINE
Mem_desc *Kip::mem_descs()
{ return offset_cast<Mem_desc*>(this, _mem_descs); }

PRIVATE inline
Mem_desc const *Kip::mem_descs() const
{ return offset_cast<Mem_desc const *>(this, _mem_descs); }

PUBLIC inline ALWAYS_INLINE
unsigned Kip::num_mem_descs() const
{ return _mem_descs_num; }

PUBLIC inline NEEDS[Kip::num_mem_descs, Kip::mem_descs] ALWAYS_INLINE
cxx::static_vector<Mem_desc>
Kip::mem_descs_a()
{ return cxx::static_vector<Mem_desc>(mem_descs(), num_mem_descs()); }

PUBLIC inline NEEDS[Kip::num_mem_descs, Kip::mem_descs] ALWAYS_INLINE
cxx::static_vector<Mem_desc const>
Kip::mem_descs_a() const
{ return cxx::static_vector<Mem_desc const>(mem_descs(), num_mem_descs()); }


PUBLIC inline
void Kip::num_mem_descs(unsigned n)
{ _mem_descs_num = n; }

PUBLIC
Mem_desc *Kip::add_mem_region(Mem_desc const &md)
{
  for (auto &m: mem_descs_a())
    if (m.type() == Mem_desc::Undefined)
      {
        m = md;
        return &m;
      }

  // Add mem region failed -- must be a Fiasco startup problem.  Bail out.
  panic("Too few memory descriptors in KIP");
}

DEFINE_GLOBAL Global_data<Kip *> Kip::global_kip;

PUBLIC static inline ALWAYS_INLINE NEEDS["config.h"]
void
Kip::init_global_kip(Kip *kip)
{
  global_kip = kip;

  kip->platform_info.is_mp = Config::Max_num_cpus > 1;
  kip->sched_granularity = Config::Scheduler_granularity;

  // check that the KIP has actually been set up
  //assert(kip->sigma0_ip && kip->root_ip && kip->user_ptr);
}

PUBLIC static inline Kip *Kip::k() { return global_kip; }

IMPLEMENT
char const *Kip::version_string() const
{
  static_assert((sizeof(Kip) & 0xf) == 0, "Invalid KIP structure size");

  return reinterpret_cast <char const *> (this) + (offset_version_strings << 4);
}

#ifndef TARGET_NAME
# define TARGET_NAME ""
#endif

asm(".section .initkip.version, \"a\", %progbits        \n\t"
    ".ascii \"" GREETING_COLOR_ANSI_TITLE
     "Welcome to the L4Re Microkernel!\\n\"\n\t"
    ".ascii \"" GREETING_COLOR_ANSI_INFO
     "L4Re Microkernel on " DISPLAY_ARCH "-" TARGET_WORD_LEN "\\n\"\n\t"
    ".ascii \""
     "Rev: " CODE_VERSION " compiled with " COMPILER "\"\n\t"
      ".ifnb " TARGET_NAME "\n\t"".ascii \" for " TARGET_NAME "\"\n\t"".endif\n\t"
      ".ifnb " CONFIG_LABEL "\n\t"".ascii \"    [" CONFIG_LABEL "]\"\n\t"".endif\n\t"
    ".ascii \"\\n"
     "Build: #" BUILD_NR " " BUILD_DATE
     GREETING_COLOR_ANSI_OFF "\\n\\000\"                \n\t"
    ".previous                                          \n\t");

asm(".section .initkip.features.end, \"a\", %progbits   \n\t"
    ".string \"\"                                       \n\t"
    ".previous                                          \n\t");


//----------------------------------------------------------------------------
IMPLEMENTATION[!sync_clock]:

IMPLEMENT inline NEEDS["mem.h"]
Cpu_time
Kip::clock() const
{ return Mem::read64_consistent(const_cast<Cpu_time const *>(&_clock)); }

IMPLEMENT inline
void
Kip::set_clock(Cpu_time c)
{ _clock = c; }

IMPLEMENT inline NEEDS["mem.h"]
void
Kip::add_to_clock(Cpu_time plus)
{
  // This function does not force a full atomic update. The caller needs to be
  // aware about this: Either the update is performed by a single specific CPU
  // (the boot CPU) or the callers have to use a lock.
  // However, on ARM < v7, the update of the 64-bit clock can be observed in
  // any order defeating the user-level retry loop for reading the clock which
  // assumes that low word is written before the high word.
  Mem::write64_consistent(const_cast<Cpu_time *>(&_clock), _clock + plus);
}
