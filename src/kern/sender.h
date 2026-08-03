#pragma once

#include "l4_types.h"
#include "prio_list.h"
#include "cpu_lock.h"
#include "lock_guard.h"

#include <cassert>

class Context;

/** A sender.  This is a role class, so real senders need to inherit from it.
 */
class Sender : private Prio_list_elem
{
  MEMBER_OFFSET();

  // allow Prio_list to deal with Sender gracefully
  friend class Prio_list;
public:
  /**
   * IPC operation flags encoding the receive phase and reply capability intent.
   *
   * Bit 0 (have_receive): a receive phase follows the send.
   * Bit 1 (want_reply_cap): the receiver should establish a reply cap for the
   *   sender on delivery, enabling the sender to enter a closed wait for the
   *   reply. Only meaningful when have_receive is set.
   *
   * Typical usage:
   *   Ipc_flags(true, true)   - call: send + closed wait for reply
   *   Ipc_flags(true, false)  - send + open/WQ wait, no reply cap
   *   Ipc_flags(false, false) - send only, no receive
   */
  struct Ipc_flags
  {
    unsigned char v = 0;
    Ipc_flags() = default;
    constexpr explicit Ipc_flags(unsigned char flags) : v(flags) {}
    constexpr bool have_receive() const { return v != 0; }
    constexpr bool want_reply_cap() const { return v & 2; }
    constexpr explicit Ipc_flags(bool have_recv, bool want_reply_cap)
    : v((have_recv ? 1 : 0) | ((have_recv && want_reply_cap) ? 2 : 0))
    {}
  };

  using Prio_list_elem::wait_queue;

  /**
   * Receiver-ready callback. Called in the receiver's context when the
   * receiver is ready to accept the sender's message.
   *
   * \param receiver      The receiving context.
   * \param set_closed_wait  If non-null, the sender's receive phase (if any)
   *   becomes a closed wait for this Sender as the reply partner, instead of
   *   accepting the first queued sender. Used by the WQ dispatch path to bind
   *   the reply to the specific receiver that dequeued the sender.
   */
  virtual void ipc_send_msg(Context *, Sender *set_closed_wait = nullptr) = 0;
  virtual void ipc_receiver_aborted() = 0;
  virtual void modify_label(Mword const *todo, int cnt) = 0;

  unsigned short sender_prio() const
  {
    return Prio_list_elem::prio();
  }

  /** Sender in a queue of senders?.
      @return true if sender has enqueued in a receiver's list of waiting
              senders
   */
  bool in_sender_list() const
  {
    return Prio_list_elem::in_list();
  }

  static bool in_sender_list(Sender const *s, Prio_list const *list)
  {
    if (auto p = list->current_poi())
      if (p == s)
        return p.queued();

    return s->Prio_list_elem::wait_queue() == list;
  }

  static bool is_head_of(Sender const *s, Prio_list const *l)
  {
    return l->first() == s;
  }

  static Sender *cast(Prio_list_elem *e)
  {
    return static_cast<Sender*>(e);
  }

  bool sender_enqueue(Prio_list *head, unsigned short prio)
  {
    assert(prio < 256);

    auto guard = lock_guard(cpu_lock);
    return head->insert(this, prio);
  }

  template< typename P_LIST >
  bool sender_dequeue(P_LIST list)
  {
    if (!in_sender_list())
      return false;

    auto guard = lock_guard(cpu_lock);
    return list->dequeue(this);
  }

  Prio_list_elem *qitem() { return this; }

protected:
  Sender() = default;

private:
  friend class Jdb;
  friend class Jdb_thread_list;
};

