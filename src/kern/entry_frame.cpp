INTERFACE:

#include "l4_types.h"

class Syscall_pre_frame {};
class Syscall_post_frame {};

/**
 * Encapsulation of syscall data.
 *
 * This class must be defined in arch dependent parts and has to represent the
 * data necessary for a system call as laid out on the kernel stack.
 *
 * The Syscall_frame is a subset of the Entry_frame and includes only those
 * registers used for passing IPC parameters.
 */
class Syscall_frame
{
public:
  /// Get destination object.
  L4_obj_ref ref() const;

  /// Set destination object.
  void ref(L4_obj_ref const &ref);

  /// Get UTCB.
  Utcb *utcb() const;

  /// Get message tag.
  L4_msg_tag tag() const;

  /// Set message tag.
  void tag(L4_msg_tag const &tag);

  /// Get_timeout.
  L4_timeout_pair timeout() const;

  /// Set timeout.
  void timeout(L4_timeout_pair const &to);

  /// Get IPC label for receiver.
  Mword from_spec() const;

  /// Set IPC label for receiver.
  void from(Mword id);
};

/**
 * Encapsulation of registers on the stack for leaving the kernel (instruction
 * pointer to return to, stack pointer, state, etc).
 *
 * The Return_frame is part of the Entry_frame.
 */
class Return_frame
{
public:
  /// Get instruction pointer.
  Mword ip() const;

  /// Set instruction pointer.
  void ip(Mword new_ip);

  /// User instruction pointer used for logging if kernel entered via syscall.
  Mword ip_syscall_user() const;

  /// Get stack pointer.
  Mword sp() const;

  /// Set stack pointer.
  void sp(Mword new_sp);

  /// Disable continuation if the Return_frame includes `eret_work` to store the
  /// address of the handler for a pending exception.
  void disable_continuation();
};

//----------------------------------------------------------------------------
INTERFACE [ia32 || amd64 || (arm && 32bit)]:

/**
 * Encapsulation of a syscall entry kernel stack.
 *
 * This class encapsulates the complete top of the kernel stack after a syscall
 * (including the IRET return frame).
 */
class Entry_frame
: public Syscall_post_frame,
  public Syscall_frame,
  public Syscall_pre_frame,
  public Return_frame
{
public:
  static Entry_frame *to_entry_frame(Syscall_frame *sf)
  { return nonull_static_cast<Entry_frame *>(sf); }

  Syscall_frame *syscall_frame() { return this; }
  Syscall_frame const *syscall_frame() const { return this; }
} __attribute__((packed));

//----------------------------------------------------------------------------
INTERFACE [(arm && 64bit) || riscv || mips]:

class Entry_frame
: public Return_frame
{
public:
  static Entry_frame *to_entry_frame(Syscall_frame *sf);
  Syscall_frame *syscall_frame();
  Syscall_frame const *syscall_frame() const;
};
