IMPLEMENTATION [arm]:

#include "globals.h"
#include "jdb.h"
#include "jdb_types.h"

PRIVATE static inline NEEDS["globals.h", "jdb.h", "jdb_types.h"]
void
Jdb_set_trace::set_ipc_entry(void (*e)())
{
  typedef void (*Sys_call)(void);
  extern Sys_call sys_call_table[];
  check(!Jdb::poke_task(Jdb_address::kmem_addr(&sys_call_table[0]), &e, sizeof(e)));
}

