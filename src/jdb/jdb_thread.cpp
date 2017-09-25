INTERFACE:

#include "thread_object.h"

class Jdb_thread
{
};

IMPLEMENTATION:

#include "irq.h"
#include "jdb_kobject.h"
#include "kobject.h"
#include "semaphore.h"
#include "thread_state.h"
#include "vlog.h"

#include <cstdio>

PUBLIC static
void
Jdb_thread::print_state_long(Thread *t, unsigned cut_on_len = 0)
{
  static char const * const state_names[] =
    {
      "ready",         "drq_rdy",       "send",        "rcv_wait",
      "rcv_in_progr",  "transfer",      "<0x40>",      "cancel",
      "timeout",       "dead",          "suspended",   "<0x800>",
      "migrate",       "resched",       "<0x4000>",    "fpu",
      "alien",         "dealien",       "exc_progr",   "<0x80000>",
      "drq",           "lock_wait",     "vcpu",        "vcpu_user",
      "vcpu_fpu_disabled", "vcpu_ext"
    };

  unsigned chars = 0;
  bool comma = false;

  Mword bits = t->state(false);

  for (unsigned i = 0; i < sizeof (state_names) / sizeof (char *);
       i++, bits >>= 1)
    {
      if (!(bits & 1))
        continue;

      if (cut_on_len)
        {
          unsigned add = strlen(state_names[i]) + comma;
          if (chars + add > cut_on_len)
            {
              if (chars < cut_on_len - 4)
                putstr(",...");
              break;
            }
          chars += add;
        }

      printf("%s%s", &","[!comma], state_names[i]);

      comma = 1;
    }
}

PUBLIC static
bool
Jdb_thread::has_partner(Thread *t)
{
  return (t->state(false) & Thread_ipc_mask) == Thread_receive_wait;
}

PUBLIC static
bool
Jdb_thread::has_snd_partner(Thread *t)
{
  return t->state(false) & Thread_send_wait;
}

PUBLIC static
void
Jdb_thread::print_snd_partner(Thread *t, int task_format = 0)
{
  if (has_snd_partner(t))
    Jdb_kobject::print_uid(Kobject::from_dbg(Kobject_dbg::pointer_to_obj(t->wait_queue())), task_format);
  else
    // receiver() not valid
    putstr("   ");
}

PUBLIC static
void
Jdb_thread::print_partner(Thread *t, int task_format = 0)
{
  Sender const *p = t->partner();

  if (!has_partner(t))
    {
      printf("%*s ", task_format, " ");
      return;
    }

  if (!p)
    {
      printf("%*s ", task_format, "-");
      return;
    }

  if (p == Semaphore::sem_partner())
    {
      printf("%*s ", task_format, "Sem");
      // Actually getting the semaphore would require to scan all
      // semaphores' wait lists to find t
    }
  else if (Kobject *o = Kobject::from_dbg(Kobject_dbg::pointer_to_obj(p)))
    {
      char flag = '?';

      if (cxx::dyn_cast<Thread*>(o))
        flag = ' ';
      else if (cxx::dyn_cast<Irq*>(o))
        flag = '*';

      printf("%*.lx%c", task_format, o->dbg_info()->dbg_id(), flag);
    }
  else
    printf("\033[31;1m%p\033[m ", p);
}
