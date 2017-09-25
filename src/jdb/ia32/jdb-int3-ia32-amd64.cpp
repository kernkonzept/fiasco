IMPLEMENTATION[ia32,amd64]:

#include "thread.h"

/**
 * Handle int3 extensions in the current thread's context. All functions
 * for which we don't need/want to switch to the debugging stack.
 * \return 0 if this function should be handled in the context of Jdb
 *         1 successfully handled
 */
PRIVATE static
int
Jdb::handle_int3_threadctx(Trap_state *ts)
{
  if (handle_int3_threadctx_generic(ts))
    return 1;

  Thread *t   = current_thread();
  Mem_space *s    = t->mem_space();
  Address ip = ts->ip();
  Jdb_entry_frame *jef = reinterpret_cast<Jdb_entry_frame*>(ts);
  Address_type user = jef->from_user();

  switch (s->peek((Unsigned8*)ip, user))
    {
    case 0x3c: // cmpb
      switch (s->peek((Unsigned8*)(ip+1), user))
	{
	case 13:
	  ts->value(Vkey::get());
	  Vkey::clear();
	  break;

	case 31: // kernel watchdog
	  switch (ts->value2())
	    {
	    case 1:
	      // enable watchdog
	      Watchdog::user_enable();
	      break;
	    case 2:
	      // disable watchdog
	      Watchdog::user_disable();
	      break;
	    case 3:
	      // user takes over the control of watchdog and is from now on
	      // responsible for calling "I'm still alive" events (function 5)
	      Watchdog::user_takeover_control();
	      break;
	    case 4:
	      // user returns control of watchdog to kernel
	      Watchdog::user_giveback_control();
    	      break;
	    case 5:
	      // I'm still alive
	      Watchdog::touch();
	      break;
	    }
	  break;
	default:
	  return 0; // => Jdb
	}
      break;

    default:
      return 0; // => Jdb
    }

  return 1;
}
