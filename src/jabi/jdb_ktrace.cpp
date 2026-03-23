INTERFACE:

#include "types.h"
#include "config.h"

/// Tracebuffer entry sequence number.
using Tb_sequence = Mword;

/// Tracebuffer entry generation number. This is an atomically accessed
/// variant of the sequence number. Defined as a separate type for future
/// type-safe atomic defintion.
using Tb_generation = Tb_sequence;

enum
{
  Kern_cnt_context_switch    = 0,
  Kern_cnt_addr_space_switch = 1,
  Kern_cnt_shortcut_failed   = 2,
  Kern_cnt_shortcut_success  = 3,
  Kern_cnt_irq               = 4,
  Kern_cnt_ipc_long          = 5,
  Kern_cnt_page_fault        = 6,
  Kern_cnt_io_fault          = 7,
  Kern_cnt_task_create       = 8,
  Kern_cnt_schedule          = 9,
  Kern_cnt_iobmap_tlb_flush  = 10,
  Kern_cnt_exc_ipc           = 11,
  Kern_cnt_max
};

struct alignas(Config::PAGE_SIZE) Tracebuffer_status
{
  unsigned version;  //< Current version of the tracebuffer layout.
  size_t alignment;  //< Current value of Config::stable_cache_alignment.

  /// Bit mask for computing tracebuffer slot index.
  alignas(Config::stable_cache_alignment) size_t mask;

  /// Latest tracebuffer entry generation number.
  alignas(Config::stable_cache_alignment) Tb_generation tail;

  Unsigned32 scaler_tsc_to_ns;
  Unsigned32 scaler_tsc_to_us;
  Unsigned32 scaler_ns_to_tsc;

  Unsigned32 kerncnts[Kern_cnt_max];
};

static_assert(alignof(Tracebuffer_status) == Config::PAGE_SIZE);
static_assert((sizeof(Tracebuffer_status) % Config::PAGE_SIZE) == 0);
