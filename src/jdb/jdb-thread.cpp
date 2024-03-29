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

IMPLEMENT_DEFAULT
Thread *
Jdb::get_thread(Cpu_number cpu)
{
  Jdb_entry_frame *c = get_entry_frame(cpu);

  return static_cast<Thread*>(context_of(c));
}

PUBLIC static inline NEEDS["thread.h"]
Space*
Jdb::get_space(Cpu_number cpu)
{
  Thread *thread = Jdb::get_thread(cpu);
  return thread ? thread->space() : nullptr;
}
