INTERFACE:

#include "thread_object.h"

class Semaphore;

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

PRIVATE static
unsigned
Jdb_thread::print_state_bits(Mword bits, unsigned max_size)
{
  static char const * const state_names[] =
    {
      "ready",         "drq_rdy",       "send",         "rcv_wait",
      "rcv_in_progr",  "transfer",      "trans_failed", "cancel",
      "timeout",       "dead",          "suspended",    "<0x800>",
      "migrate",       "resched",       "<0x4000>",     "fpu",
      "alien",         "dealien",       "exc_progr",    "<0x80000>",
      "drq",           "lock_wait",     "vcpu",         "vcpu_user",
      "vcpu_fpu_disabled", "vcpu_ext"
    };

  unsigned chars = 0;
  bool comma = false;

  for (unsigned i = 0; i < sizeof (state_names) / sizeof (char *);
       i++, bits >>= 1)
    {
      if (!(bits & 1))
        continue;

      if (max_size)
        {
          unsigned add = strlen(state_names[i]) + comma;
          if (chars + add > max_size)
            {
              if (chars + 4 <= max_size)
                putstr(",...");
              return 0;
            }
          chars += add;
        }

      printf("%s%s", &","[!comma], state_names[i]);

      comma = 1;
    }

  return chars < max_size ? max_size - chars : 0;
}

PUBLIC static
void
Jdb_thread::print_state_long(Thread *t, unsigned max_size = 119)
{
  max_size = print_state_bits(t->state(false), max_size);
  if (!t->_remote_state_change.pending())
    return;

  if (max_size > 7)
    {
      putstr(" [add: ");
      max_size = print_state_bits(t->_remote_state_change.add, max_size - 7);
    }
  if (max_size > 7)
    {
      putstr("; del: ");
      max_size = print_state_bits(t->_remote_state_change.del, max_size - 7);
    }
  if (max_size > 0)
    putchar(']');
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

PRIVATE static
Semaphore const *
Jdb_thread::waiting_for_semaphore(Thread *t)
{
  for (auto const &o : Kobject_dbg::_kobjects)
    if (auto const *s = cxx::dyn_cast<Semaphore*>(Kobject::from_dbg(o)))
      if (t->wait_queue() == s->waiting())
        return s;

  return nullptr;
}

PUBLIC static
void
Jdb_thread::print_partner(Thread *t, int task_format = 0)
{
  if (!has_partner(t))
    {
      printf("%*s ", task_format, " ");
      return;
    }

  void const *p = t->_partner;
  if (!p)
    printf("%*s ", task_format, "-");
  else if (t->is_partner(Semaphore::sem_partner()))
    {
      if (Semaphore const *waiting_for = waiting_for_semaphore(t))
        printf("%*lx+", task_format, waiting_for->dbg_id());
      else
        printf("%*s ", task_format, "Sem");
    }
  else if (Kobject *o = Kobject::from_dbg(Kobject_dbg::pointer_to_obj(p)))
    {
      char flag;
      if (cxx::dyn_cast<Thread*>(o))
        flag = ' ';
      else if (cxx::dyn_cast<Irq*>(o))
        flag = '*';
      else
        flag = '?';
      printf("%*.lx%c", task_format, o->dbg_info()->dbg_id(), flag);
    }
  else
    printf("\033[31;1m%p\033[m ", p);
}
