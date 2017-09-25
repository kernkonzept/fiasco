INTERFACE:

#include "initcalls.h"
#include "l4_types.h"
#include "thread.h"

class Tb_entry;
class String_buffer;

class Jdb_tbuf_output
{
private:
  typedef void (Format_entry_fn)(String_buffer *, Tb_entry *tb, const char *tidstr,
                                 int tidlen);
  static Format_entry_fn *_format_entry_fn[];
  static bool show_names;
};

IMPLEMENTATION:

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "initcalls.h"
#include "jdb.h"
#include "jdb_kobject_names.h"
#include "jdb_regex.h"
#include "jdb_symbol.h"
#include "jdb_tbuf.h"
#include "kdb_ke.h"
#include "kernel_console.h"
#include "l4_types.h"
#include "processor.h"
#include "static_init.h"
#include "string_buffer.h"
#include "tb_entry.h"
#include "terminate.h"
#include "watchdog.h"


Jdb_tbuf_output::Format_entry_fn *Jdb_tbuf_output::_format_entry_fn[Tbuf_dynentries];
bool Jdb_tbuf_output::show_names;

static void
console_log_entry(Tb_entry *e, const char *)
{
  static String_buf<80> log_message;
  log_message.reset();

  // disable all interrupts to stop other threads
  Watchdog::disable();
  Proc::Status s = Proc::cli_save();

  Jdb_tbuf_output::print_entry(&log_message, e);
  printf("%s\n", log_message.begin());

  // do not use getchar here because we ran cli'd
  // and getchar may do halt
  int c;
  Jdb::enter_getchar();
  while ((c=Kconsole::console()->getchar(false)) == -1)
    Proc::pause();
  Jdb::leave_getchar();
  if (c == 'i')
    kdb_ke("LOG");
  if (c == '^')
    terminate(1);

  // enable interrupts we previously disabled
  Proc::sti_restore(s);
  Watchdog::enable();
}

PRIVATE static
void
Jdb_tbuf_output::dummy_format_entry(String_buffer *buf, Tb_entry *tb, const char *, int)
{
  buf->printf(" << no format_entry_fn for type %d registered >>", tb->type());
}

STATIC_INITIALIZE(Jdb_tbuf_output);

PUBLIC static
void FIASCO_INIT
Jdb_tbuf_output::init()
{
  unsigned i;

  Jdb_tbuf::direct_log_entry = &console_log_entry;
  for (i=0; i<sizeof(_format_entry_fn)/sizeof(_format_entry_fn[0]); i++)
    if (!_format_entry_fn[i])
      _format_entry_fn[i] = dummy_format_entry;
}

PUBLIC static
void
Jdb_tbuf_output::register_ff(Unsigned8 type, Format_entry_fn format_entry_fn)
{
  assert(type < Tbuf_dynentries);

  if (   (_format_entry_fn[type] != 0)
      && (_format_entry_fn[type] != dummy_format_entry))
    panic("format entry function for type %d already registered", type);

  _format_entry_fn[type] = format_entry_fn;
}

// return thread+ip of entry <e_nr>
PUBLIC static
int
Jdb_tbuf_output::thread_ip(int e_nr, Thread const **th, Mword *ip)
{
  Tb_entry *e = Jdb_tbuf::lookup(e_nr);

  if (!e)
    return false;

  *th = static_cast<Thread const *>(e->ctx());

  *ip = e->ip();

  return true;
}

PUBLIC static
void
Jdb_tbuf_output::toggle_names()
{
  show_names = !show_names;
}

static
void
formatter_default(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  if (tb->type() < Tbuf_dynentries)
    return;

  int idx = tb->type()-Tbuf_dynentries;

  Tb_entry_formatter *fmt = _jdb_log_table[idx].fmt;
  char const *sc = _jdb_log_table[idx].name;
  sc += strlen(sc) + 1;

  buf->printf("%-3s: %-*s ", sc, tidlen, tidstr);

  if (!fmt)
    {
      Tb_entry_ke_reg *e = static_cast<Tb_entry_ke_reg*>(tb);
      buf->printf("\"%s\" " L4_PTR_FMT " " L4_PTR_FMT " " L4_PTR_FMT " @ " L4_PTR_FMT,
               e->msg.str(), e->v[0], e->v[1], e->v[2], e->ip());
      return;
    }

  fmt->print(buf, tb);
}

PUBLIC static
void
Jdb_tbuf_output::print_entry(String_buffer *buf, int e_nr)
{
  Tb_entry *tb = Jdb_tbuf::lookup(e_nr);

  if (tb)
    print_entry(buf, tb);
}

PUBLIC static
void
Jdb_tbuf_output::print_entry(String_buffer *buf, Tb_entry *tb)
{
  assert(tb->type() < Tbuf_max);

  char tidstr[32];
  Thread const *t = static_cast<Thread const *>(tb->ctx());

  if (!t || !Kobject_dbg::is_kobj(t))
    strcpy(tidstr, "????");
  else
    {
      Jdb_kobject_name *ex
        = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(t);
      if (show_names && ex)
        snprintf(tidstr, sizeof(tidstr), "%04lx %-*.*s", t->dbg_info()->dbg_id(), (int)ex->max_len(), (int)ex->max_len(), ex->name());
      else
        snprintf(tidstr, sizeof(tidstr), "%04lx", t->dbg_info()->dbg_id());
    }

  if (Config::Max_num_cpus > 1)
    buf->printf(Config::Max_num_cpus > 16 ? "%02d " : "%d ", tb->cpu());

  if (tb->type() >= Tbuf_dynentries)
    formatter_default(buf, tb, tidstr, show_names ? 21 : 4);
  else
    _format_entry_fn[tb->type()](buf, tb, tidstr, show_names ? 21 : 4);

  buf->fill(' ');
  buf->terminate();
}

PUBLIC static
bool
Jdb_tbuf_output::set_filter(const char *filter_str, Mword *entries)
{
  if (*filter_str && Jdb_regex::avail() && !Jdb_regex::start(filter_str))
    return false;

  if (!*filter_str)
    {
      for (Mword n=0; n<Jdb_tbuf::unfiltered_entries(); n++)
	Jdb_tbuf::unfiltered_lookup(n)->unhide();

      Jdb_tbuf::disable_filter();
      if (entries)
	*entries = Jdb_tbuf::unfiltered_entries();
      return true;
    }

  Mword cnt = 0;

  for (Mword n=0; n<Jdb_tbuf::unfiltered_entries(); n++)
    {
      Tb_entry *e = Jdb_tbuf::unfiltered_lookup(n);
      String_buf<200> s;

      print_entry(&s, e);
      if (Jdb_regex::avail())
	{
	  if (Jdb_regex::find(s.begin(), 0, 0))
	    {
	      e->unhide();
	      cnt++;
	      continue;
	    }
	}
      else
	{
	  if (strstr(s.begin(), filter_str))
	    {
	      e->unhide();
	      cnt++;
	      continue;
	    }
	}
      e->hide();
    }

  if (entries)
    *entries = cnt;
  Jdb_tbuf::enable_filter();
  return true;
}
