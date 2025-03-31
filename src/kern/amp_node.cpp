INTERFACE:

#include <cxx/cxx_int>
#include "std_macros.h"

/**
 * AMP node physical ID.
 *
 * The physical ID is an opaque number. They still have a defined difference
 * between each other, so that the range
 *
 *   [Amp_node::first_node(), Amp_node::first_node() + Amp_node::Max_num_nodes - 1]
 *
 * is properly defined.
 */
struct Amp_phys_id
: cxx::int_type_base<unsigned, Amp_phys_id, unsigned>
{
  Amp_phys_id() = default;
  constexpr explicit Amp_phys_id(unsigned id)
  : cxx::int_type_base<unsigned, Amp_phys_id, unsigned>(id)
  {}
};

class Amp_node
{
public:
  /**
   * The physical node-id of the first CPU.
   *
   * Usually the numbering starts at 0. A platform might override the
   * implementation if numbering is different.
   */
  static constexpr Amp_phys_id first_node();

  /**
   * Get physical node-id of current CPU.
   *
   * *Must* be an inline function that is completely inlineable. It's used
   * by Platform_control::amp_boot_info_idx() which is called without a
   * stack!
   */
  static inline Amp_phys_id phys_id();

  /**
   * Get logical node-id of current CPU.
   *
   * The boot node logical id is 0. Other nodes start at 1, up to Max_num_nodes
   * - 1.
   *
   * *Must* be an inline function that is completely inlineable. It's used
   * by Platform_control::amp_boot_info_idx() which is called without a
   * stack!
   */
  static inline unsigned id();
};

//---------------------------------------------------------------------------
INTERFACE[amp]:

#include <globalconfig.h>

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_num_nodes = CONFIG_MP_MAX_CPUS;
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT_DEFAULT static constexpr
Amp_phys_id
Amp_node::first_node()
{ return Amp_phys_id(0); }

IMPLEMENT inline ALWAYS_INLINE
unsigned
Amp_node::id()
{
  return phys_id() - first_node();
}

/**
 * Convert logical node-ID to physical node-ID.
 */
PUBLIC static constexpr
Amp_phys_id
Amp_node::phys_id(unsigned node)
{ return first_node() + node; }

//---------------------------------------------------------------------------
IMPLEMENTATION[!amp]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_num_nodes = 1;
};

IMPLEMENT inline ALWAYS_INLINE
Amp_phys_id
Amp_node::phys_id()
{ return first_node(); }
