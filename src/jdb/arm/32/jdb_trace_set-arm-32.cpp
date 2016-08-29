IMPLEMENTATION [arm]:

PRIVATE static inline
void
Jdb_set_trace::set_ipc_entry(void (*e)())
{
  typedef void (*Sys_call)(void);
  extern Sys_call sys_call_table[];
  sys_call_table[2] = e;
}

