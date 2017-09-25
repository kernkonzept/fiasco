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
      1,              // version
      0,              // flags
      0,              // kmemsize
      { 0, 0, 0, 0, 0, 0 }, // uart
      "",             // jdb_cmd
      0,              // tbuf_entries
      0,              // out_buf
    };
}

namespace Koptions {
  Options *o() { return &Koptions_ns::o; }
};
