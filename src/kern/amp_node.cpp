INTERFACE:

class Amp_node
{
public:
  /**
   * Get node-id of current CPU.
   *
   * *Must* be an inline function that is completely inlineable. It's used
   * by Platform_control::amp_boot_info_idx() which is called without a
   * stack!
   */
  static inline unsigned current_node();
};

//---------------------------------------------------------------------------
INTERFACE[amp]:

#include <globalconfig.h>

EXTENSION class Amp_node
{
public:
  // The platform is supposed to define `First_node`.
  enum : unsigned { Max_num_nodes = CONFIG_MP_MAX_CPUS };
};

//---------------------------------------------------------------------------
IMPLEMENTATION[!amp]:

EXTENSION class Amp_node
{
public:
  enum : unsigned { First_node = 0, Max_num_nodes = 1 };
};

IMPLEMENT inline
unsigned
Amp_node::current_node()
{ return First_node; }
