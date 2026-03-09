INTERFACE:

#include "koptions-def.h"
#include "std_macros.h"

namespace Koptions {
  using namespace L4_kernel_options;

  Options const *o() FIASCO_CONST;
};

IMPLEMENTATION:

#include "amp_node.h"

namespace Koptions_ns
{
  constexpr Koptions::Options get_options(int node)
  {
    return Koptions::Options
             {
               Koptions::Magic,
               Koptions::Version_current,
               0,                              // flags
               0,                              // kmemsize
               { 0, 0, 0, 0, 0, 0, "" },       // uart
               0,                              // core_spin_addr
               "",                             // jdb_cmd
               0,                              // tbuf_entries
               0,                              // out_buf
               cxx::int_value<Amp_phys_id>(Amp_node::phys_id(node)),
             };
  }

  template<unsigned NODES>
  struct Amp_koptions
  {
    Amp_koptions<NODES - 1> next;
    Koptions::Options options = get_options(NODES - 1);
  };

  template<>
  struct Amp_koptions<1>
  { Koptions::Options options = get_options(0); };

  Amp_koptions<Amp_node::Max_num_nodes> const
  o __attribute__((section(".koptions")));
}

namespace Koptions {
  Options const *o()
  {
    Options const *first = reinterpret_cast<Options const *>(&Koptions_ns::o);
    return first + Amp_node::id();
  }
};
