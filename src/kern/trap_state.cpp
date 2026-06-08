INTERFACE:

/**
 * The stack frame, if the kernel was entered due to a raised exception.
 */
class Trap_state
{
public:
  using Handler = FIASCO_FASTCALL int (*)(Trap_state *, Cpu_number cpu);

  /// Provide information for a page fault exception.
  void set_pagefault(Mword pfa, Mword error);

  /// Return the number of the exception.
  Mword trapno() const;

  /// Return the error code of the exception.
  Mword error() const;

  /// Return the instruction pointer where the exception was triggered.
  Mword ip() const;

  /// Modify the instruction pointer where the exception was triggered.
  void ip(Mword new_ip);

  /// Return the stack pointer when the exception was triggered.
  Mword sp() const;

  /// Return true if exceptions of this type are not added to the tracebuffer.
  bool exclude_logging() const;

  /// Dump for debugging purposes.
  void dump() const;
};

/**
 * The register state transferred via UTCB to/from the exception handler.
 */
struct Trex
{
  Trap_state s;

  void set_ipc_upcall();

  /// Dump for debugging purposes.
  void dump()
  { s.dump(); }
};

