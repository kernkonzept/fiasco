
#pragma once

#include "kobject.h"
#include "thread.h"

class Obj_cap : public L4_obj_ref
{
public:
  Obj_cap(L4_obj_ref const &o) : L4_obj_ref(o) {}

  Kobject_iface *deref(L4_fpage::Rights *rights = nullptr,
                       [[maybe_unused]] bool dbg = false)
  {
    Thread *current = current_thread();
    if (op() & L4_obj_ref::Ipc_reply)
      {
        // Reply + closed-recv with a valid cap index: resolve cap index as
        // the receive target (e.g. Wait_queue). WQ::invoke handles the reply
        // internally via reply_cap(). Only triggers for the new combination
        // Ipc_send|Ipc_reply|Ipc_recv without Ipc_open_wait.
        if ((op() & L4_obj_ref::Ipc_recv) && !(op() & L4_obj_ref::Ipc_open_wait)
            && !special())
          return current->space()->lookup_local(cap(), L4_fpage::Rights::NONE())
                   .deref_nocheck(nullptr, rights);

        Context::Reply_cap reply_cap = current->reply_cap();
        if (rights) *rights = reply_cap.rights();
        Thread *ca = static_cast<Thread*>(reply_cap.receiver());
        return ca;
      }

    if (EXPECT_FALSE(special()))
      {
        if (!self())
          return nullptr;

        if (rights) *rights = L4_fpage::Rights::CRWS();
        return current;
      }

    return current->space()->lookup_local(cap(), L4_fpage::Rights::NONE())
      .deref_nocheck(nullptr, rights);
  }
};

