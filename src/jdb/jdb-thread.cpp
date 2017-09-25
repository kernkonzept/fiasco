INTERFACE:

class Space;

EXTENSION class Jdb
{
public:
  /**
   * Deliver Thread object which was active at entry of kernel debugger.
   * If we came from the kernel itself, return Thread with id 0.0
   */
  static Thread *get_thread(Cpu_number cpu);
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "jdb_prompt_ext.h"
#include "jdb.h"
#include "thread.h"
#include "mem_layout.h"

class Jdb_tid_ext : public Jdb_prompt_ext
{
public:
  void ext();
  void update();
};

IMPLEMENT
void
Jdb_tid_ext::ext()
{
  if (Jdb::get_current_active())
    printf("(%p) ", Jdb::get_current_active());
}

IMPLEMENT
void
Jdb_tid_ext::update()
{
  Jdb::get_current(Jdb::current_cpu);
}

//static Jdb_tid_ext jdb_tid_ext INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

IMPLEMENT_DEFAULT
Thread *
Jdb::get_thread(Cpu_number cpu)
{
  Jdb_entry_frame *c = get_entry_frame(cpu);

  return static_cast<Thread*>(context_of(c));
}

PUBLIC
static void
Jdb::get_current(Cpu_number cpu)
{
  current_active = get_thread(cpu);
}

PUBLIC static inline NEEDS["thread.h"]
Space*
Jdb::get_current_space()
{
  return current_active ? current_active->space() : 0;
}
