IMPLEMENTATION:

#include <climits>
#include <cstring>
#include <cstdio>

#include "jdb.h"
#include "jdb_core.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_thread.h"
#include "jdb_kobject_names.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "minmax.h"
#include "simpleio.h"
#include "task.h"
#include "thread.h"
#include "thread_state.h"
#include "static_init.h"

class Jdb_thread_list : public Jdb_module
{
public:
  Jdb_thread_list() FIASCO_INIT;
private:
  static char subcmd;
  static char long_output;
  static Cpu_number cpu;

private:
  static int _mode;
  static int _count;
  static char _pr;
  static Thread *_t_head, *_t_start;

  friend class _foo;

  enum { LIST_UNSORTED, LIST_SORT_PRIO, LIST_SORT_TID, LIST_SORT_SPACE,
         LIST_SORT_END };

};

char Jdb_thread_list::subcmd;
char Jdb_thread_list::long_output;
Cpu_number Jdb_thread_list::cpu;


// available from the jdb_tcb module
extern int jdb_show_tcb(Thread* thread, int level)
  __attribute__((weak));

char Jdb_thread_list::_pr;
int  Jdb_thread_list::_mode = LIST_SORT_TID;
int  Jdb_thread_list::_count;

Thread *Jdb_thread_list::_t_head;
Thread *Jdb_thread_list::_t_start;

PUBLIC static
void
Jdb_thread_list::init(char pr, Thread *t_head)
{
  _pr = pr;
  _t_head = t_head;
}

// return string describing current sorting mode of list
PUBLIC static inline NOEXPORT
const char*
Jdb_thread_list::get_mode_str(void)
{
  static const char * const mode_str[] =
    { "(unsorted)", "(prio-sorted)", "(tid-sorted)", "(space-sorted)" };

  return mode_str[_mode];
}

// switch to next sorting mode
PUBLIC static
void
Jdb_thread_list::switch_mode(void)
{
  if (++_mode >= LIST_SORT_END)
    _mode = LIST_UNSORTED;
}

// set _t_start element of list
PUBLIC static
void
Jdb_thread_list::set_start(Thread *t_start)
{
  _t_start = t_start;
  iter(+Jdb_screen::height()-3, &_t_start);
  iter(-Jdb_screen::height()+3, &_t_start);
}

// _t_start-- if possible
PUBLIC static
int
Jdb_thread_list::line_back(void)
{
  return iter(-1, &_t_start);
}

// _t_start++ if possible
PUBLIC static
int
Jdb_thread_list::line_forw(void)
{
  Thread *t = _t_start;
  iter(+Jdb_screen::height()-2, &_t_start);
  iter(-Jdb_screen::height()+3, &_t_start);
  return t != _t_start;
}

// _t_start -= 24 if possible
PUBLIC static
int
Jdb_thread_list::page_back(void)
{
  return iter(-Jdb_screen::height()+2, &_t_start);
}

// _t_start += 24 if possible
PUBLIC static
int
Jdb_thread_list::page_forw(void)
{
  Thread *t = _t_start;
  iter(+Jdb_screen::height()*2-5, &_t_start);
  iter(-Jdb_screen::height()  +3, &_t_start);
  return t != _t_start;
}

// _t_start = first element of list
PUBLIC static
int
Jdb_thread_list::goto_home(void)
{
  return iter(-9999, &_t_start);
}

// _t_start = last element of list
PUBLIC static
int
Jdb_thread_list::goto_end(void)
{
  Thread *t = _t_start;
  iter(+9999, &_t_start);
  iter(-Jdb_screen::height()+2, &_t_start);
  return t != _t_start;
}

// search index of t_search starting from _t_start
PUBLIC static
int
Jdb_thread_list::lookup(Thread *t_search)
{
  unsigned i;
  Thread *t;

  for (i=0, t=_t_start; i<Jdb_screen::height()-3; i++)
    {
      if (t == t_search)
	break;
      iter(+1, &t);
    }

  return i;
}

// get y'th element of thread list starting from _t_start
PUBLIC static
Thread*
Jdb_thread_list::index(int y)
{
  Thread *t = _t_start;

  iter(y, &t);
  return t;
}

// helper function for iter() -- use priority as sorting key
static
long
Jdb_thread_list::get_prio(Thread *t)
{
  return t->sched()->prio();
}

// helper function for iter() -- use thread id as sorting key
static
long
Jdb_thread_list::get_tid(Thread *t)
{
  return t->dbg_info()->dbg_id();
}

// helper function for iter() -- use space as sorting key
static
long
Jdb_thread_list::get_space_dbgid(Thread *t)
{
  return Kobject_dbg::pointer_to_id(t->space());
}


// --------------------------------------------------------------------------
IMPLEMENTATION [sched_wfq || sched_fp_wfq]:

template<typename T> class Jdb_thread_list_policy;

template<typename RQP>
static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_wfq_iter_prev(Sched_context *t)
{
  Sched_context::Ready_queue &rq = Sched_context::rq.cpu(cpu);
  Sched_context **rl = RQP::link(t);
  if (!rl || rl == RQP::idle(rq))
    return RQP::cnt(rq) ? RQP::heap(rq)[RQP::cnt(rq) - 1] : *RQP::idle(rq);

  if (rl == RQP::heap(rq))
    return *RQP::idle(rq);

  return *(rl - 1);
}

template<typename RQP>
static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_wfq_iter_next(Sched_context *t)
{
  Sched_context::Ready_queue &rq = Sched_context::rq.cpu(cpu);
  Sched_context **rl = RQP::link(t);
  if (!rl || rl == RQP::idle(rq))
    return RQP::cnt(rq) ? RQP::heap(rq)[0] : *RQP::idle(rq);

  if ((unsigned)(rl - RQP::heap(rq)) >= RQP::cnt(rq))
    return *RQP::idle(rq);

  return *(rl + 1);
}


// --------------------------------------------------------------------------
IMPLEMENTATION [sched_fixed_prio || sched_fp_wfq]:

template<typename T> struct Jdb_thread_list_policy;

template<typename RQP>
static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_fp_iter_prev(Sched_context *t)
{
  unsigned prio = RQP::prio(t);
  Sched_context::Ready_queue &rq = Sched_context::rq.cpu(cpu);

  if (t != RQP::prio_next(rq, prio))
    return RQP::prev(t);

  for (;;)
    {
      if (++prio > RQP::prio_highest(rq))
	prio = 0;
      if (RQP::prio_next(rq, prio))
	return RQP::prev(RQP::prio_next(rq, prio));
    }
}

template<typename RQP>
static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_fp_iter_next(Sched_context *t)
{
  unsigned prio = RQP::prio(t);
  Sched_context::Ready_queue &rq = Sched_context::rq.cpu(cpu);

  if (RQP::next(t) != RQP::prio_next(rq, prio))
    return RQP::next(t);

  for (;;)
    {
      if (--prio > RQP::prio_highest(rq)) // prio is unsigned
	prio = RQP::prio_highest(rq);
      if (RQP::prio_next(rq, prio))
	return RQP::prio_next(rq, prio);
    }
}

// --------------------------------------------------------------------------
IMPLEMENTATION [sched_fixed_prio]:

template<>
struct Jdb_thread_list_policy<Ready_queue_fp<Sched_context> >
{
  typedef Ready_queue_fp<Sched_context> Rq;

  static unsigned prio(Sched_context *t)
  { return t->prio(); }

  static Sched_context *prio_next(Sched_context::Ready_queue &rq, unsigned prio)
  { return rq.prio_next[prio].front(); }

  static unsigned prio_highest(Sched_context::Ready_queue &rq)
  { return rq.prio_highest; }

  static Sched_context *prev(Sched_context *t)
  { return *--Rq::List::iter(t); }

  static Sched_context *next(Sched_context *t)
  { return *++Rq::List::iter(t); }
};

static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_iter_prev(Sched_context *t)
{ return sc_fp_iter_prev<Jdb_thread_list_policy<Ready_queue_fp<Sched_context> > >(t); }

static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_iter_next(Sched_context *t)
{ return sc_fp_iter_next<Jdb_thread_list_policy<Ready_queue_fp<Sched_context> > >(t); }

// --------------------------------------------------------------------------
IMPLEMENTATION [sched_wfq]:

template<>
class Jdb_thread_list_policy<Ready_queue_wfq<Sched_context> >
{
public:
  static Sched_context **link(Sched_context *t)
  { return t->_ready_link; }

  static Sched_context **heap(Sched_context::Ready_queue &rq)
  { return rq._heap; }

  static Sched_context **idle(Sched_context::Ready_queue &rq)
  { return &rq.idle; }

  static unsigned cnt(Sched_context::Ready_queue &rq)
  { return rq._cnt; }
};

static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_iter_prev(Sched_context *t)
{ return sc_wfq_iter_prev<Jdb_thread_list_policy<Ready_queue_wfq<Sched_context> > >(t); }

static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_iter_next(Sched_context *t)
{ return sc_wfq_iter_next<Jdb_thread_list_policy<Ready_queue_wfq<Sched_context> > >(t); }


// --------------------------------------------------------------------------
IMPLEMENTATION [sched_fp_wfq]:

static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_iter_prev(Sched_context *)
{
  return 0;
}

static inline NOEXPORT
Sched_context *
Jdb_thread_list::sc_iter_next(Sched_context *)
{
  return 0;
}


// --------------------------------------------------------------------------
IMPLEMENTATION:


static inline NOEXPORT
Thread*
Jdb_thread_list::iter_prev(Thread *t)
{
  if (_pr == 'p')
    {
      Kobject_dbg::Iterator o = Kobject_dbg::Kobject_list::iter(t->dbg_info());
      do
	{
	  --o;
	  if (o == Kobject_dbg::end())
	    --o;
	}
      while (!cxx::dyn_cast<Thread*>(Kobject::from_dbg(*o)));
      return cxx::dyn_cast<Thread*>(Kobject::from_dbg(*o));
    }
  else
    return static_cast<Thread*>(sc_iter_prev(t->sched())->context());
}


static inline NOEXPORT
Thread*
Jdb_thread_list::iter_next(Thread *t)
{
  if (_pr == 'p')
    {
      Kobject_dbg::Iterator o = Kobject_dbg::Kobject_list::iter(t->dbg_info());
      do
	{
	  ++o;
	  if (o == Kobject_dbg::end())
	    ++o;
	}
      while (!cxx::dyn_cast<Thread*>(Kobject::from_dbg(*o)));
      return cxx::dyn_cast<Thread*>(Kobject::from_dbg(*o));
    }
  else
    return static_cast<Thread*>(sc_iter_next(t->sched())->context());
}

// walk though list <count> times
// abort walking if no more elements
// do iter if iter != 0
static
bool
Jdb_thread_list::iter(int count, Thread **t_start,
		      void (*iter)(Thread *t)=0)
{
  int i = 0;
  int forw = (count >= 0);
  Thread *t, *t_new = *t_start, *t_head = _t_head;
  long (*get_key)(Thread *t) = 0;

  if (count == 0)
    return false;  // nothing changed

  if (count < 0)
    count = -count;

  // if we are stepping backwards, begin at end-of-list
  if (!forw)
    t_head = iter_prev(t_head);

  switch (_mode)
    {
    case LIST_UNSORTED:
      // list threads in order of list
      if (iter)
	iter(*t_start);

      t = *t_start;
      do
	{
	  t = forw ? iter_next(t) : iter_prev(t);

	  if (t == t_head)
	    break;

	  if (iter)
	    iter(t);

	  t_new = t;
	  i++;

	} while (i < count);
      break;

    case LIST_SORT_PRIO:
      // list threads sorted by priority
    case LIST_SORT_SPACE:
      // list threads sorted by space
      if (!get_key)
	get_key = (_mode == LIST_SORT_SPACE) ? get_space_dbgid : get_prio;

      // fall through

    case LIST_SORT_TID:
      // list threads sorted by thread id
	{
	  long key;
	  int start_skipped = 0;

	  if (!get_key)
	    get_key = get_tid;

	  long key_current = get_key(*t_start);
	  long key_next = (forw) ? LONG_MIN : LONG_MAX;
          t = t_head;
	  if (iter)
	    iter(*t_start);
	  do
	    {
	      if (t == *t_start)
		start_skipped = 1;

	      key = get_key(t);
	      // while walking through the current list, look for next key
	      if (   ( forw && (key > key_next) && (key < key_current))
		  || (!forw && (key < key_next) && (key > key_current)))
		key_next = key;

              if (t_head == (t = (forw) ? iter_next(t) : iter_prev(t)))
		{
		  if (   ( forw && (key_next == LONG_MIN))
		      || (!forw && (key_next == LONG_MAX)))
		    break;
		  key_current = key_next;
		  key_next = forw ? LONG_MIN : LONG_MAX;
                }

	      if (start_skipped && (get_key(t) == key_current))
		{
		  if (iter)
		    iter(t);

		  i++;
		  t_new = t;
		}
	    } while (i < count);
	}
      break;
    }

  _count = i;

  bool changed = (*t_start != t_new);
  *t_start = t_new;

  return changed;
}

// show complete page using show callback function
PUBLIC static
int
Jdb_thread_list::page_show(void (*show)(Thread *t))
{
  Thread *t = _t_start;

  iter(Jdb_screen::height()-3, &t, show);
  return _count;
}

// show complete list using show callback function
PUBLIC static
int
Jdb_thread_list::complete_show(void (*show)(Thread *t))
{
  Thread *t = _t_start;

  iter(9999, &t, show);
  return _count;
}

IMPLEMENT
Jdb_thread_list::Jdb_thread_list()
  : Jdb_module("INFO")
{}

PUBLIC
Jdb_module::Action_code
Jdb_thread_list::action(int cmd, void *&argbuf, char const *&fmt, int &)
{
  static char const *const cpu_fmt = " cpu=%i\n";
  static char const *const nfmt = "";
  if (cmd == 0)
    {
      if (fmt != cpu_fmt && fmt != nfmt)
	{
	  if (subcmd == 'c')
	    {
	      argbuf = &cpu;
	      fmt = cpu_fmt;
	    }
	  else
	    fmt = nfmt;
	  return EXTRA_INPUT;
	}

      Thread *t = Jdb::get_current_active();
      switch (subcmd)
	{
        case 'r': cpu = Cpu_number::first(); list_threads(t, 'r'); break;
	case 'p': list_threads(t, 'p'); break;
	case 'c':
		  if (Cpu::online(cpu))
		    list_threads(Jdb::get_thread(cpu), 'r');
		  else
		    printf("\nCPU %u is not online!\n", Cpu_number::val(cpu));
		  cpu = Cpu_number::first();
		  break;
	case 't': Jdb::execute_command("lt"); break; // other module
	case 's': Jdb::execute_command("ls"); break; // other module
	}
    }
  else if (cmd == 1)
    {
      Thread *t = Jdb::get_current_active();
      long_output = 1;
      Jdb_thread_list::init(subcmd == 'r' ? 'r' : 'p', t);
      Jdb_thread_list::set_start(t);
      Jdb_thread_list::goto_home();
      Jdb_thread_list::complete_show(list_threads_show_thread);
      long_output = 0;
    }

  return NOTHING;
}

PRIVATE static inline
void
Jdb_thread_list::print_thread_name(Kobject_common const * o, int len)
{
  Jdb_kobject_name *nx = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o);

  if (nx)
    {
      len = min((int)nx->max_len(), len);
      printf("%-*.*s", len, len, nx->name());
    }
  else
    printf("%-*.*s", len, len, "-----");
}

static void
Jdb_thread_list::list_threads_show_thread(Thread *t)
{
  char to[24];
  int  waiting_for = 0;
  int  plen = 0;

  *to = '\0';

  Kconsole::console()->getchar_chance();

  Jdb_kobject::print_uid(t, 5);
  plen += 5;

  plen += printf(" %3u", cxx::int_value<Cpu_number>(t->home_cpu()));

  if (t->home_cpu() != t->get_current_cpu())
    plen += printf(":%-3u ", cxx::int_value<Cpu_number>(t->get_current_cpu()));
  else
    plen += printf("     ");

  print_thread_name(t, 15);
  plen += 15;

  plen += printf("  %2lx ", (unsigned long)get_prio(t));

  if (get_space_dbgid(t) == ~0L)
    plen += printf(" ----- ");
  else
    plen += printf(" %5lx ", Kobject_dbg::pointer_to_id(t->space()));

  if (Jdb_thread::has_partner(t))
    {
      Jdb_thread::print_partner(t, 5);
      waiting_for = 1;
    }
  else if (Jdb_thread::has_snd_partner(t))
    {
      Jdb_thread::print_snd_partner(t, 5);
      putchar(' ');
      waiting_for = 1;
    }
  else
    putstr("      ");
  plen += 6;

  if (waiting_for)
    {
      if (t->_timeout && t->_timeout->is_set())
	{
	  Signed64 diff = (t->_timeout->get_timeout(Kip::k()->clock));
	  if (diff < 0)
	    strcpy(to, " over");
	  else if (diff >= 100000000LL)
	    strcpy(to, " >99s");
	  else
	    {
	      int us = (int)diff;
	      if (us < 0)
		us = 0;
	      if (us >= 1000000)
		snprintf(to, sizeof(to), " %3ds", us / 1000000);
	      else if (us >= 1000)
		snprintf(to, sizeof(to), " %3dm", us / 1000);
	      else
		snprintf(to, sizeof(to), " %3d%c", us, Config::char_micro);
	    }
	}
    }

  plen += printf("%-6s", to);

  if (long_output)
    {
      Jdb_thread::print_state_long(t, 47);
      putchar('\n');
    }
  else
    {
      if (Config::Stack_depth)
	{
	  Mword i, stack_depth;
	  char *c  = (char*)t + sizeof (Thread);
	  for (i = sizeof (Thread), stack_depth = Context::Size;
	      i < Context::Size;
	      i++, stack_depth--, c++)
	    if (*c != '5')
	      break;

	  plen += printf("(%4lu) ", stack_depth - sizeof (Thread));
	}

      if ((int)Jdb_screen::width() > plen)
	Jdb_thread::print_state_long(t, Jdb_screen::width() - plen);
      putstr("\033[K\n");
    }
}

static void
Jdb_thread_list::show_header()
{
  Jdb::cursor();
  printf("%s   id  cpu    name             pr     sp  wait    to%s state\033[m\033[K",
         Jdb::esc_emph, Config::Stack_depth ? "  stack" : "");
}

static void
Jdb_thread_list::list_threads(Thread *t_start, char pr)
{
  unsigned y, y_max;
  Thread *t, *t_current = t_start;

    {
      // Hm, we are in JDB, however we have to make the assertion in
      // ready_enqueue happy.
      auto g = lock_guard(cpu_lock);
      // enqueue current, which may not be in the ready list due to lazy queueing
      if (!t_current->in_ready_list())
        Sched_context::rq.cpu(t_current->home_cpu()).ready_enqueue(t_current->sched());
    }

  Jdb::clear_screen();
  show_header();
  Jdb_thread_list::init(pr, t_current);

  for (;;)
    {
      Jdb_thread_list::set_start(t_current);

      // set y to position of t_current in current displayed list
      y = Jdb_thread_list::lookup(t_current);

      for (bool resync=false; !resync;)
	{
	  Jdb::cursor(2, 1);
	  y_max = Jdb_thread_list::page_show(list_threads_show_thread);

	  // clear rest of screen (if where less than 24 lines)
	  for (unsigned i=y_max; i < Jdb_screen::height()-3; i++)
            putstr("\033[K\n");

	  Jdb::printf_statline(pr=='r' ? "ready list" : "present list",
			       "<Space>=mode " /*"<Tab>=partner "*/ "<CR>=select",
			       "%-15s", Jdb_thread_list::get_mode_str());

	  // key event loop
	  for (bool redraw=false; !redraw; )
	    {
	      Jdb::cursor(y+2, 6);
	      switch (int c=Jdb_core::getchar())
		{
		case KEY_CURSOR_UP:
		case 'k':
		  if (y > 0)
		    y--;
		  else
		    redraw = Jdb_thread_list::line_back();
		  break;
		case KEY_CURSOR_DOWN:
		case 'j':
		  if (y < y_max)
		    y++;
		  else
		    redraw = Jdb_thread_list::line_forw();
		  break;
		case KEY_PAGE_UP:
		case 'K':
		  if (!(redraw = Jdb_thread_list::page_back()))
		    y = 0;
		  break;
		case KEY_PAGE_DOWN:
		case 'J':
		  if (!(redraw = Jdb_thread_list::page_forw()))
		    y = y_max;
		  break;
		case KEY_CURSOR_HOME:
		case 'H':
		  redraw = Jdb_thread_list::goto_home();
		  y = 0;
		  break;
		case KEY_CURSOR_END:
		case 'L':
		  redraw = Jdb_thread_list::goto_end();
		  y = y_max;
		  break;
		case ' ': // switch mode
		  t_current = Jdb_thread_list::index(y);
		  Jdb_thread_list::switch_mode();
		  redraw = true;
		  resync = true;
		  break;
#if 0
		case KEY_TAB: // goto thread we are waiting for
		  t = Jdb_thread_list::index(y);
		  if (t->partner()
		      && (t->state(false) & (Thread_receiving |
					Thread_busy  |
					Thread_rcvlong_in_progress))
		      && (!t->partner()->id().is_irq() ||
		           t->partner()->id().irq() > Config::Max_num_dirqs))
		    {
		      t_current = static_cast<Thread*>(t->partner());
		      redraw = true;
		      resync = true;
		    }
		  break;
#endif
		case KEY_RETURN: // show current tcb
		case KEY_RETURN_2:
		  if (jdb_show_tcb != 0)
		    {
		      t = Jdb_thread_list::index(y);
		      if (!jdb_show_tcb(t, 1))
			return;
		      show_header();
		      redraw = 1;
		    }
		  break;
		case KEY_ESC:
		  Jdb::abort_command();
		  return;
		default:
		  if (Jdb::is_toplevel_cmd(c)) 
		    return;
		}
	    }
	}
    }
}


PUBLIC
Jdb_module::Cmd const *
Jdb_thread_list::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "l", "list", "%C", "l{r|p}\tshow ready/present list", &subcmd },
        { 1, "", "threadlist", "%C", 0 /* invisible */, &subcmd },
    };

  return cs;
}

PUBLIC
int
Jdb_thread_list::num_cmds() const
{
  return 2;
}

static Jdb_thread_list jdb_list_threads INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
