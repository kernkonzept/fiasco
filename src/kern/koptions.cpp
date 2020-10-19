INTERFACE:

#include "koptions-def.h"

namespace Koptions {
  using namespace L4_kernel_options;

  Options *o();
};

IMPLEMENTATION:

namespace Koptions_ns
{
  Koptions::Options o __attribute__((section(".koptions"))) =
    {
      Koptions::Magic,
      Koptions::Version_current,
      0,              // flags
      0,              // kmemsize
      { 0, 0, 0, 0, 0, 0 }, // uart
      0,              // core_spin_addr
      "",             // jdb_cmd
      0,              // tbuf_entries
      0,              // out_buf
    };
}

namespace Koptions {
  Options *o() { return &Koptions_ns::o; }
};
