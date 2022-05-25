INTERFACE:

#include "koptions-def.h"

namespace Koptions {
  using namespace L4_kernel_options;

  Options const *o();
};

IMPLEMENTATION:

namespace Koptions_ns
{
  Koptions::Options const o __attribute__((section(".koptions"))) =
    {
      Koptions::Magic,
      Koptions::Version_current,
      0,              // flags
      0,              // kmemsize
      { 0, { 0 }, 0, 0, 0, 0, 0, 0 }, // uart
      0,              // core_spin_addr
      "",             // jdb_cmd
      0,              // tbuf_entries
      0,              // out_buf
      0,              // node
    };
}

namespace Koptions {
  Options const *o() { return &Koptions_ns::o; }
};
