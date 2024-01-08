INTERFACE:

#include "l4_types.h"
#include "config.h"

class Tss
{
public:
  /**
   * Hardware-defined portion of the TSS.
   *
   * \note The context part of the TSS is not allowed to cross a page
   *       boundary to avoid CPU errata. Thus we align the whole TSS structure
   *       to the page size.
   */

  struct Ctx
  {
    Unsigned32 back_link;
    Address esp0;
    Unsigned32 ss0;
    Address esp1;
    Unsigned32 ss1;
    Address esp2;
    Unsigned32 ss2;
    Unsigned32 cr3;
    Unsigned32 eip;
    Unsigned32 eflags;
    Unsigned32 eax;
    Unsigned32 ecx;
    Unsigned32 edx;
    Unsigned32 ebx;
    Unsigned32 esp;
    Unsigned32 ebp;
    Unsigned32 esi;
    Unsigned32 edi;
    Unsigned32 es;
    Unsigned32 cs;
    Unsigned32 ss;
    Unsigned32 ds;
    Unsigned32 fs;
    Unsigned32 gs;
    Unsigned32 ldt;
    Unsigned16 trace_trap;
    Unsigned16 iopb;
  } __attribute__((packed));

  struct Io
  {
    Config::Io_bitmap bitmap;
    Unsigned8 bitmap_delimiter;
  } __attribute__((packed));

  struct Hw
  {
    Ctx ctx;
    Io io;
  };

  enum : size_t
  {
    /**
     * TSS segment limit (the highest addressable offset in the segment).
     *
     * The TSS segment limit is a bound check for the CPU, thus it only covers
     * the hardware-defined portion of the TSS. The software-defined portion of
     * the TSS can still be accessed directly.
     *
     */
    Segment_limit = sizeof(Hw) - 1,

    /**
     * TSS segment limit without the IO bitmap.
     *
     * The TSS segment limit is a bound check for the CPU, thus it only covers
     * the hardware-defined portion of the TSS (in this case only the context
     * part, disabling the IO bitmap).
     */
    Segment_limit_sans_io = sizeof(Ctx) - 1
  };

  Hw _hw;

  /**
   * Software-defined portion of the TSS.
   */

  Mword _io_bitmap_revision;
} __attribute__((aligned(Config::PAGE_SIZE)));

IMPLEMENTATION:

static_assert(offsetof(Tss, _hw.io.bitmap)
              < (1 << (sizeof(Tss::_hw.ctx.iopb) * 8)));

PUBLIC inline
void
Tss::set_ss0(unsigned ss)
{ _hw.ctx.ss0 = ss; }
