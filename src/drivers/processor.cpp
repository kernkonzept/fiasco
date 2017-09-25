INTERFACE:

#include "types.h"

/// Central processor specific methods.
class Proc
{
public:

  typedef Mword Status;

  /// Block external interrupts
  static void cli();

  /// Unblock external inetrrupts
  static void sti();

  /// Are external interrupts enabled ?
  static Status interrupts();

  /// Block external interrupts and save the old state
  static Status cli_save();

  /// Conditionally unblock external interrupts
  static void sti_restore( Status );

  static void pause();

  static void halt();

  static void irq_chance();

  static void stack_pointer(Mword sp);

  static Mword stack_pointer();

  static Mword program_counter();

  static inline
  void preemption_point()
  {
    sti();
    irq_chance();
    cli();
  }

};
