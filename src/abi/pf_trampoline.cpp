INTERFACE [pagefault_trampoline]:

#include "trap_state.h"

struct Pf_trampoline
{
  enum : Mword
  {
    /**
     * The corresponding thread is currently handling a page fault using the
     * trampoline handler.
     *
     * Userland must not be ex-regs'd the thread during that time!
     */
    Pf_in_progress = 1 << 0,

    /**
     * If the corresponding thread is currently handling a page fault using the
     * page fault trampoline handler, trigger an exception immediately after the
     * thread resumed from page fault handling.
     */
    Trigger_exception_after_resume = 1 << 1,
  };

  /// Page fault handler address.
  Mword ip;

  /// User-specific data.
  Mword user_data[2];

  /**
   * Utcb::values[0] when page fault happened.
   *
   * The kernel has to restore the userland trampoline PF handler re-entered the
   * kernel Utcb::values[0] set to the respective opcode (Pf_tramp_op::Resume or
   * Pf_tramp_op::Reflect).
   */
  Mword saved_mr0;

  /// Control flags.
  Mword flags;

  /// State of thread at page fault.
  Trap_state ts;
};
