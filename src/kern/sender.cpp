INTERFACE:

#include "l4_types.h"
#include "prio_list.h"

class Receiver;

/** A sender.  This is a role class, so real senders need to inherit from it.
 */
class Sender : private Prio_list_elem
{
  MEMBER_OFFSET();
public:
  /**
   * Receiver-ready callback. Receivers call this function on waiting senders
   * when they get ready to receive a message from that sender.
   *
   * \param receiver   Receiver that receives the message from the waiting sender.
   * \param open_wait  Whether the receiver is in a open or closed wait.
   *
   * \pre Interrupts must be disabled.
   * \post This function is a potential preemption point. In particular,
   *       this means that upon return, the Sender might may already have been
   *       deleted unless the caller holds a counted reference to it.
   */
  virtual void ipc_send_msg(Receiver *receiver, bool open_wait) = 0;

  /**
   * Receivers call this function on waiting senders when they get want to
   * reject a message from that sender.
   *
   * \pre Interrupts must be disabled.
   * \post This function is a potential preemption point. In particular,
   *       this means that upon return, the Sender might may already have been
   *       deleted unless the caller holds a counted reference to it.
   */
  virtual void ipc_receiver_aborted() = 0;

  virtual void modify_label(Mword const *todo, int cnt) = 0;

protected:
  Sender() = default;
  Prio_list *_wq;

private:

  friend class Jdb;
  friend class Jdb_thread_list;
};


IMPLEMENTATION:

#include <cassert>

#include "atomic.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "mem.h"

//
// state requests/manipulation
//

/** Optimized constructor.  This constructor assumes that the object storage
    is zero-initialized.
    @param ignored an integer argument. The value doesn't matter and
                   is used just to distinguish this constructor from the
		   default one.
 */
PROTECTED inline
explicit
Sender::Sender (int /*ignored*/)
: _wq(nullptr)
{}


/** Current receiver.
    @return receiver this sender is currently trying to send a message to.
 */
PUBLIC inline
Prio_list *
Sender::wait_queue() const
{ return _wq; }

/** Set current receiver.
    @param receiver the receiver we're going to send a message to
 */
PUBLIC inline
void
Sender::set_wait_queue(Prio_list *wq)
{
  _wq = wq;
}

PUBLIC inline
unsigned short Sender::sender_prio()
{
  return Prio_list_elem::prio();
}

/** Sender in a queue of senders?.
    @return true if sender has enqueued in a receiver's list of waiting
            senders
 */
PUBLIC inline
bool
Sender::in_sender_list() const
{
  return Prio_list_elem::in_list();
}

PUBLIC inline
bool
Sender::is_head_of(Prio_list const *l) const
{ return l->first() == this; }

PUBLIC static inline
Sender *
Sender::cast(Prio_list_elem *e)
{ return static_cast<Sender*>(e); }

PUBLIC
void Sender::sender_enqueue(Prio_list *head, unsigned short prio)
{
  assert(!in_sender_list());
  assert(prio < 256);

  auto guard = lock_guard(cpu_lock);
  head->insert(this, prio);
}

PUBLIC template< typename P_LIST >
void Sender::sender_dequeue(P_LIST list)
{
  if (!in_sender_list())
    return;

  auto guard = lock_guard(cpu_lock);
  list->dequeue(this);
}
