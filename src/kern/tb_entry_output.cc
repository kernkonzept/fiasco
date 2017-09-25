#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "jdb_symbol.h"
#include "jdb_tbuf_output.h"
#include "jdb_util.h"
#include "kobject_dbg.h"
#include "static_init.h"
#include "tb_entry.h"
#include "thread.h"
#include "string_buffer.h"


// timeout => x.x{
static
void
format_timeout(String_buffer *buf, Mword us)
{
  if (us >= 1000000000)		// =>100s
    buf->printf(">99s");
  else if (us >= 10000000)	// >=10s
    buf->printf("%lus", us/1000000);
  else if (us >= 1000000)	// >=1s
    buf->printf("%lu.%lus", us/1000000, (us%1000000)/100000);
  else if (us >= 10000)		// 10ms
    buf->printf("%lum", us/1000);
  else if (us >= 1000)		// 1ms
    buf->printf("%lu.%lum", us/1000, (us%1000)/100);
  else
    buf->printf("%lu%c", us, Config::char_micro);
}

template< typename T >
static
char const *
get_ipc_type(T e)
{
  switch (e->ipc_type())
    {
    case L4_obj_ref::Ipc_call:           return "call";
    case L4_obj_ref::Ipc_call_ipc:       return "call ipc";
    case L4_obj_ref::Ipc_send:           return "send";
    case L4_obj_ref::Ipc_recv:           return "recv";
    case L4_obj_ref::Ipc_send_and_wait:  return "snd wait";
    case L4_obj_ref::Ipc_reply_and_wait: return "repl";
    case L4_obj_ref::Ipc_wait:           return "wait";
    case L4_obj_ref::Ipc_send | L4_obj_ref::Ipc_reply:
                                         return "send rcap";
    default:                             return "unk ipc";
    }
}

static char const * const __tag_interpreter_strings_l4re[] = {
    "ds", // 0
    "name",
    "parent",
    "goos",
    "ma",
    "rm", // 5
    "ev",
  };
static char const * const __tag_interpreter_strings_fiasco[] = {
    "Kirq",      // -1
    "Kpf",
    "Kpreempt",
    "Ksysexc",
    "Kexc",      // -5
    "Ksigma",
    0,
    "Kiopf",
    "Kcapfault",
    0,           // -10
    "Ktask",
    "Kthread",
    "Klog",
    "Ksched",
    "Kfactory",  // -15
    "Kvm",
    0,
    0,
    0,
    "Ksem",      // -20
    "Kmeta",
  };

static
char const *
tag_to_string(L4_msg_tag const &tag)
{
  if (0x4000 <= tag.proto() && tag.proto() <= 0x4006)
    return __tag_interpreter_strings_l4re[tag.proto() - 0x4000];
  if (-21L <= tag.proto() && tag.proto() <= -1)
    return __tag_interpreter_strings_fiasco[-tag.proto() - 1];
  return 0;
}

static
void
print_msgtag(String_buffer *buf, L4_msg_tag const &tag)
{
  if (char const *s = tag_to_string(tag))
    buf->printf("%s%04lx", s, tag.raw() & 0xffff);
  else
    buf->printf(L4_PTR_FMT, tag.raw());
}

class Tb_entry_ipc_fmt : public Tb_entry_formatter
{
public:
  Tb_entry_ipc_fmt() {}
  void print(String_buffer *, Tb_entry const *) const {}
  Group_order has_partner(Tb_entry const *) const
  { return Tb_entry::Group_order::first(); }
  Group_order is_pair(Tb_entry const *e, Tb_entry const *n) const
  {
    if (static_cast<Tb_entry_ipc_res const *>(n)->pair_event() == e->number())
      return Group_order::last();
    else
      return Group_order::none();
  }
  Mword partner(Tb_entry const *) const { return 0; }
};

class Tb_entry_ipc_res_fmt : public Tb_entry_formatter
{
public:
  Tb_entry_ipc_res_fmt() {}
  void print(String_buffer *, Tb_entry const *) const {}
  Group_order has_partner(Tb_entry const *) const
  { return Tb_entry::Group_order::last(); }
  Group_order is_pair(Tb_entry const *e, Tb_entry const *n) const
  {
    if (static_cast<Tb_entry_ipc_res const *>(e)->pair_event() == n->number())
      return Group_order::first();
    else
      return Group_order::none();
  }
  Mword partner(Tb_entry const *e) const { return static_cast<Tb_entry_ipc_res const *>(e)->pair_event(); }
};


// ipc / irq / shortcut success
static
void
formatter_ipc(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_ipc *e = static_cast<Tb_entry_ipc*>(tb);
  unsigned char type = e->ipc_type();
  if (!type)
    type = L4_obj_ref::Ipc_call_ipc;

  // ipc operation / shortcut succeeded/failed
  const char *m = get_ipc_type(e);

  buf->printf("%s: ", (e->type()==Tbuf_ipc) ? "ipc" : "sc ");
  buf->printf("%-*s %s->", tidlen, tidstr, m);

  // print destination id
  if (e->dst().special())
    buf->printf("[C:INV] DID=%lx", e->dbg_id());
  else
    buf->printf("[C:%lx] DID=%lx", e->dst().raw(), e->dbg_id());

  buf->printf(" L=%lx", e->label());

  // print mwords if have send part
  if (type & L4_obj_ref::Ipc_send)
    {
      buf->printf(" [");

      print_msgtag(buf, e->tag());

      buf->printf("] (" L4_PTR_FMT "," L4_PTR_FMT ")", e->dword(0), e->dword(1));
    }

  buf->printf(" TO=");
  L4_timeout_pair to = e->timeout();
  if (type & L4_obj_ref::Ipc_send)
    {
      if (to.snd.is_absolute())
	{
	  // absolute send timeout
	  Unsigned64 end = 0; // FIXME: to.snd.microsecs_abs (e->kclock());
	  format_timeout(buf, (Mword)(end > e->kclock() ? end-e->kclock() : 0));
	}
      else
	{
	  // relative send timeout
	  if (to.snd.is_never())
	    buf->printf("INF");
	  else if (to.snd.is_zero())
	    buf->printf("0");
	  else
            format_timeout(buf, (Mword)to.snd.microsecs_rel(0));
	}
    }
  if (type & L4_obj_ref::Ipc_send
      && type & L4_obj_ref::Ipc_recv)
    buf->printf("/");
  if (type & L4_obj_ref::Ipc_recv)
    {
      if (to.rcv.is_absolute())
	{
	  // absolute receive timeout
	  Unsigned64 end = 0; // FIXME: to.rcv.microsecs_abs (e->kclock());
	  format_timeout(buf, (Mword)(end > e->kclock() ? end-e->kclock() : 0));
	}
      else
	{
	  // relative receive timeout
	  if (to.rcv.is_never())
	    buf->printf("INF");
	  else if (to.rcv.is_zero())
	    buf->printf("0");
	  else
            format_timeout(buf, (Mword)to.rcv.microsecs_rel(0));
	}
    }
}

// result of ipc
static
void
formatter_ipc_res(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_ipc_res *e = static_cast<Tb_entry_ipc_res*>(tb);
  L4_error error;
  if (e->tag().has_error())
    error = e->result();
  else
    error = L4_error::None;
  const char *m = "answ"; //get_ipc_type(e);

  buf->printf("     %s%-*s %s [%08lx] L=%lx err=%lx (%s) (%lx,%lx) ",
			    e->is_np() ? "[np] " : "",
			    tidlen, tidstr, m, e->tag().raw(), e->from(),
                            error.raw(), error.str_error(), e->dword(0),
			    e->dword(1));
}


// pagefault
static
void
formatter_pf(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_pf *e = static_cast<Tb_entry_pf*>(tb);
  buf->printf("pf:  %-*s pfa=" L4_PTR_FMT " ip=" L4_PTR_FMT " (%c%c) spc=%p err=%lx",
      tidlen, tidstr, e->pfa(), e->ip(),
      !PF::is_read_error(e->error()) ? (e->error() & 4 ? 'w' : 'W')
                                     : (e->error() & 4 ? 'r' : 'R'),
      !PF::is_translation_error(e->error()) ? 'p' : '-',
      e->space(), e->error());
}

// pagefault
void
Tb_entry_pf::print(String_buffer *buf) const
{
  buf->printf("pfa=" L4_PTR_FMT " ip=" L4_PTR_FMT " (%c%c) spc=%p",
      pfa(), ip(),
      !PF::is_read_error(error()) ? (error() & 4 ? 'w' : 'W')
                                  : (error() & 4 ? 'r' : 'R'),
      !PF::is_translation_error(error()) ? 'p' : '-',
      space());
}

// kernel event (enter_kdebug("*..."))
static
void
formatter_ke(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_ke *e = static_cast<Tb_entry_ke*>(tb);
  buf->printf("ke:  %-*s \"%s\" @ " L4_PTR_FMT,
              tidlen, tidstr, e->msg.str(), e->ip());
}

// kernel event (enter_kdebug_verb("*+...", val1, val2, val3))
static
void
formatter_ke_reg(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_ke_reg *e = static_cast<Tb_entry_ke_reg*>(tb);
  buf->printf("ke:  %-*s \"%s\" "
      L4_PTR_FMT " " L4_PTR_FMT " " L4_PTR_FMT " @ " L4_PTR_FMT,
      tidlen, tidstr, e->msg.str(), e->v[0], e->v[1], e->v[2],
      e->ip());
}


// breakpoint
static
void
formatter_bp(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_bp *e = static_cast<Tb_entry_bp*>(tb);
  buf->printf("b%c:  %-*s @ " L4_PTR_FMT " ",
              "iwpa"[e->mode() & 3], tidlen, tidstr, e->ip());
  switch (e->mode() & 3)
    {
    case 1:
    case 3:
      switch (e->len())
	{
     	case 1:
	  buf->printf("[" L4_PTR_FMT "]=%02lx", e->addr(), e->value());
	  break;
	case 2:
	  buf->printf("[" L4_PTR_FMT "]=%04lx", e->addr(), e->value());
	  break;
	case 4:
	  buf->printf("[" L4_PTR_FMT "]=" L4_PTR_FMT, e->addr(), e->value());
	  break;
	}
      break;
    case 2:
      buf->printf("[" L4_PTR_FMT "]", e->addr());
      break;
    }
}


// trap
void
Tb_entry_trap::print(String_buffer *buf) const
{
  if (!cs())
    buf->printf("#%02x: err=%08x @ " L4_PTR_FMT,
                (unsigned)trapno(), (unsigned)error(), ip());
  else
    buf->printf(trapno() == 14
		  ? "#%02x: err=%04x @ " L4_PTR_FMT
		    " cs=%04x sp=" L4_PTR_FMT " cr2=" L4_PTR_FMT
		  : "#%02x: err=%04x @ " L4_PTR_FMT
		    " cs=%04x sp=" L4_PTR_FMT " eax=" L4_PTR_FMT,
	        (unsigned)trapno(),
		(unsigned)error(), ip(), (unsigned)cs(), sp(),
		trapno() == 14 ? cr2() : eax());
}

// kernel event log binary data
static
void
formatter_ke_bin(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_ke_bin *e = static_cast<Tb_entry_ke_bin*>(tb);
  buf->printf("ke:  %-*s BINDATA @ " L4_PTR_FMT, tidlen, tidstr, e->ip());
}

static Tb_entry_ipc_fmt const _ipc_fmt;
static Tb_entry_ipc_res_fmt const _ipc_res_fmt;

// register all available format functions
static FIASCO_INIT
void
init_formatters()
{
  Jdb_tbuf_output::register_ff(Tbuf_pf, formatter_pf);
  Jdb_tbuf_output::register_ff(Tbuf_ipc, formatter_ipc);
  Tb_entry_formatter::set_fixed(Tbuf_ipc, &_ipc_fmt);
  Jdb_tbuf_output::register_ff(Tbuf_ipc_res, formatter_ipc_res);
  Tb_entry_formatter::set_fixed(Tbuf_ipc_res, &_ipc_res_fmt);
  Jdb_tbuf_output::register_ff(Tbuf_ke, formatter_ke);
  Jdb_tbuf_output::register_ff(Tbuf_ke_reg, formatter_ke_reg);
  Jdb_tbuf_output::register_ff(Tbuf_breakpoint, formatter_bp);
  Jdb_tbuf_output::register_ff(Tbuf_ke_bin, formatter_ke_bin);
}

STATIC_INITIALIZER(init_formatters);
