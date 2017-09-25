INTERFACE:

#include "l4_types.h"

class Syscall_pre_frame {};
class Syscall_post_frame {};

/**
 * Encapsulation of syscall data.
 *
 * This class must be defined in arch dependent parts
 * and has to represent the data necessary for a 
 * system call as laid out on the kernel stack. 
 */
class Syscall_frame
{
public:

  L4_obj_ref ref() const;
  void ref(L4_obj_ref const &ref);
  Utcb *utcb() const;
  L4_msg_tag tag() const;
  void tag(L4_msg_tag const &tag);

  L4_timeout_pair timeout() const;
  void timeout(L4_timeout_pair const &to);

  Mword from_spec() const;
  void from(Mword id);
};


class Return_frame
{
public:
  Mword ip() const;
  Mword ip_syscall_page_user() const;
  void  ip(Mword _pc);

  Mword sp() const;
  void  sp(Mword _sp);
};

/**
 * Encapsulation of a syscall entry kernel stack.
 *
 * This class encapsulates the complete top of the 
 * kernel stack after a syscall (including the 
 * iret return frame).
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


