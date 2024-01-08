INTERFACE[amd64]:

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
    Unsigned32 ign0; // ignored
    Address rsp0;
    Address rsp1;
    Address rsp2;
    Unsigned32 ign1; // ignored
    Unsigned32 ign2; // ignored
    Address ist1;
    Address ist2;
    Address ist3;
    Address ist4;
    Address ist5;
    Address ist6;
    Address ist7;
    Unsigned32 ign3; // ignored
    Unsigned32 ign4; // ignored
    Unsigned16 ign5; // ignored
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
    Segment_limit = sizeof(Hw) - 1
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
Tss::set_ss0(unsigned)
{}

//-
