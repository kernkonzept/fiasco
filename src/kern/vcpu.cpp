INTERFACE:

#include "entry_frame.h"
#include "trap_state.h"
#include "vcpu_host_regs.h"

class Vcpu_state
{
  MEMBER_OFFSET();
public:
  enum State
  {
    F_irqs        = 0x1,
    F_page_faults = 0x2,
    F_exceptions  = 0x4,
    F_user_mode   = 0x20,
    F_fpu_enabled = 0x80,
    F_traps       = F_irqs | F_page_faults | F_exceptions,
  };

  enum Sticky_flags
  {
    Sf_irq_pending = 0x01,
  };

  /// vCPU ABI version (must be checked by the user for equality).
  Mword version;
  /// user-specific data
  Mword user_data[7];

  Trex _regs;
  Syscall_frame _ipc_regs;

  Unsigned16 state;
  Unsigned16 _saved_state;
  Unsigned16 sticky_flags;
  Unsigned16 _reserved;

  L4_obj_ref user_task;

  Mword _entry_sp;
  Mword _entry_ip;

  // kernel-internal private state
  Mword _sp;
  Vcpu_host_regs host;
};

// ---------------------------------------------------------------------
INTERFACE [irq_direct_inject]:

#include <cxx/dlist>

class Vcpu_irq_list_item : public cxx::D_list_item
{
public:
  /**
   * Start of interrupt.
   *
   * Consume pending interrupt by vCPU.
   */
  virtual void vcpu_soi() = 0;

  /**
   * End of interrupt by the guest.
   */
  virtual void vcpu_eoi() = 0;

  /**
   * Get architecture specific guest interrupt configuration.
   */
  virtual Mword vcpu_irq_id() const = 0;
};

typedef cxx::D_list<Vcpu_irq_list_item> Vcpu_irq_list;

// ---------------------------------------------------------------------
INTERFACE [!irq_direct_inject]:

struct Vcpu_irq_list_item {};
