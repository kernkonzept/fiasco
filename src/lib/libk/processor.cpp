INTERFACE:

#include "types.h"

/// Central processor specific methods.
class Proc
{
public:
  enum {
    Is_32bit = sizeof(Mword) == 4,
    Is_64bit = sizeof(Mword) == 8,
  };

  typedef Mword Status;

  /// Block external interrupts
  static void cli();

  /// Unblock external interrupts
  static void sti();

  /// Are external interrupts enabled?
  static Status interrupts();

  /// Block external interrupts and save the old state
  static Status cli_save();

  /// Conditionally unblock external interrupts
  static void sti_restore(Status);

  static void pause();

  static void halt();

  static void irq_chance();

  static Mword stack_pointer();

  /// Optimized version of stack_pointer() without 'volatile': The exact value
  /// is not important but it is only used to determine the current context.
  /// During context switch, all registers are either clobbered or saved and
  /// restored, ensuring the right value in the new context. This optimized
  /// version allows the compiler to remove such a function call if the value is
  /// not needed.
  static Mword stack_pointer_for_context();

  static Mword program_counter();

  static inline
  void preemption_point()
  {
    sti();
    irq_chance();
    cli();
  }

};
