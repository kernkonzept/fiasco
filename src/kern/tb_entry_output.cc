#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include "config.h"
#include "jdb_tbuf_output.h"
#include "jdb_util.h"
#include "kobject_dbg.h"
#include "static_init.h"
#include "tb_entry.h"
#include "task.h"
#include "thread.h"
#include "scheduler.h"
#include "semaphore.h"
#include "string_buffer.h"
#include "vlog.h"


static
void
format_timeout(String_buffer *buf, Signed64 us, bool prefix)
{
  if (us <= -100'000'000)
    {
      buf->printf("<");
      us = -99'000'000;
    }
  else if (us >= 100'000'000)
    {
      buf->printf(">");
      us = 99'000'000;
    }
  else if (prefix)
    buf->printf("=");

  if (us < 0)
    {
      us = -us;
      buf->printf("-");
    }
  // avoid 64-bit arithmetic on 32-bit hosts
  Unsigned32 us32 = static_cast<Unsigned32>(us);
  if (us32 >= 10'000'000)
    // 10...99s
    buf->printf("%us", us32 / 1'000'000);
  else if (us32 >= 1'000'000)
    {
      // 1.0...9.9s
      Unsigned32 deci_s = us32 / 100'000;
      buf->printf("%u.%us", deci_s / 10, deci_s % 10);
    }
  else if (us32 >= 10'000)
    // 10...999ms
    buf->printf("%um", us32 / 1'000);
  else if (us32 >= 1'000)
    {
      // 1.0...9.9ms
      Unsigned32 deci_ms = us32 / 100;
      buf->printf("%u.%um", deci_ms / 10, deci_ms % 10);
    }
  else
    // 0...999us
    buf->printf("%uu", us32);
}

template< typename T >
static
char const *
get_ipc_type(T e)
{
  switch (e->ipc_type())
    {
    case L4_obj_ref::None:               return "none";
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
    "ds", // 0x4000
    "name",
    "parent",
    "goos",
    "ma",
    "rm", // 0x4005
    "ev",
    "inh",
    "dmaspc",
    "mmiospc",
  };
static char const * const __tag_interpreter_strings_fiasco[] = {
    "Kirq",      // -1
    "Kpf",
    "Kpreempt",
    "Ksysexc",
    "Kexc",      // -5
    "Ksigma",
    nullptr,
    "Kiopf",
    "Kcapfault",
    "Kobj",      // -10
    "Ktask",
    "Kthread",
    "Klog",
    "Ksched",
    "Kfactory",  // -15
    "Kvm",
    "Kdmaspc",
    "Kirqsnd",
    "Kirqmux",
    "Ksem",      // -20
    "Kmeta",
    "Kiommu",
    "Kdbg",
    "Ksmc",
  };

static
char const *
tag_to_string(L4_msg_tag const &tag)
{
  if (0x4000 <= tag.proto() && tag.proto() <= 0x4009)
    return __tag_interpreter_strings_l4re[tag.proto() - 0x4000];
  if (-24L <= tag.proto() && tag.proto() <= -1)
    return __tag_interpreter_strings_fiasco[-tag.proto() - 1];
  return nullptr;
}

static
void
print_msgtag(String_buffer *buf, L4_msg_tag const &tag)
{
  char const msg_tag_flags[4] = { 'F', 'S', 'P', 'E' };
  char flag_buf[5];
  int i, fb = 0;
  for (i = 0; i < 4; ++i)
    if (tag.raw() & (1 << (i + 12)))
      flag_buf[fb++] = msg_tag_flags[i];
  flag_buf[fb] = 0;

  if (char const *s = tag_to_string(tag))
    buf->printf("%s", s);
  else
    buf->printf("%lx", tag.proto());

  buf->printf(",%s%sw=%d,i=%d",
              flag_buf, fb ? "," : "", tag.words(), tag.items());
}

static
void
print_words(String_buffer *buf, L4_msg_tag const &tag, Mword dw0, Mword dw1)
{
  if (tag.words() == 0)
    {
      if (tag.proto() == L4_msg_tag::Label_irq)
        buf->printf(" (trigger)");
    }
  else
    {
#define TB_ENTRY(c, o) case c::Op::o: buf->printf(#o); break
      buf->printf(" (");
      bool print_dw0 = false;
      if (tag.proto() == L4_msg_tag::Label_irq)
        switch (Icu_h_base::Op{dw0})
          {
            TB_ENTRY(Icu_h_base, Bind);
            TB_ENTRY(Icu_h_base, Unbind);
            TB_ENTRY(Icu_h_base, Info);
            TB_ENTRY(Icu_h_base, Msi_info);
            TB_ENTRY(Icu_h_base, Unmask);
            TB_ENTRY(Icu_h_base, Mask);
            TB_ENTRY(Icu_h_base, Set_mode);
            default: print_dw0 = true; break;
          }
      else if (tag.proto() == L4_msg_tag::Label_task)
        switch (Task::Op{dw0})
          {
            TB_ENTRY(Task, Map);
            TB_ENTRY(Task, Unmap);
            TB_ENTRY(Task, Cap_info);
            TB_ENTRY(Task, Add_ku_mem);
            TB_ENTRY(Task, Ldt_set_x86);
            TB_ENTRY(Task, Vgicc_map_arm);
            default: print_dw0 = true; break;
          }
      else if (tag.proto() == L4_msg_tag::Label_thread)
        switch (Thread::Op{dw0})
          {
            TB_ENTRY(Thread, Control);
            TB_ENTRY(Thread, Ex_regs);
            TB_ENTRY(Thread, Switch);
            TB_ENTRY(Thread, Vcpu_resume);
            TB_ENTRY(Thread, Register_del_irq);
            TB_ENTRY(Thread, Modify_senders);
            TB_ENTRY(Thread, Vcpu_control);
#if defined(ARCH_x86) || defined(ARCH_AMD64)
            TB_ENTRY(Thread, Gdt_x86);
#elif defined (ARCH_ARM)
            TB_ENTRY(Thread, Set_tpidruro_arm);
#endif
            TB_ENTRY(Thread, Set_segment_base_amd64);
            TB_ENTRY(Thread, Segment_info_amd64);
            default: print_dw0 = true; break;
          }
      else if (tag.proto() == L4_msg_tag::Label_log)
        switch (Vlog::Op{dw0})
          {
          case Vlog::Op::Write:
              buf->printf("Write,len=%ld,'...')", dw1);
              return;
            TB_ENTRY(Vlog, Read);
            TB_ENTRY(Vlog, Set_attr);
            TB_ENTRY(Vlog, Get_attr);
            default: print_dw0 = true; break;
          }
      else if (tag.proto() == L4_msg_tag::Label_scheduler)
        switch (Scheduler::Op{dw0})
          {
            TB_ENTRY(Scheduler, Info);
            TB_ENTRY(Scheduler, Run_thread);
            TB_ENTRY(Scheduler, Idle_time);
            default: print_dw0 = true; break;
          }
      else if (tag.proto() == L4_msg_tag::Label_irq_sender)
        switch (Irq_sender::Op{dw0})
          {
            TB_ENTRY(Irq_sender, Attach);
            TB_ENTRY(Irq_sender, Detach);
            TB_ENTRY(Irq_sender, Bind);
            default: print_dw0 = true; break;
          }
      else if (tag.proto() == L4_msg_tag::Label_semaphore)
        switch (Semaphore::Op{dw0})
          {
            TB_ENTRY(Semaphore, Down);
            default: print_dw0 = true; break;
          }
      else
        print_dw0 = true;
      if (print_dw0)
        buf->printf("%lx", dw0);
      if (tag.words() > 1)
        {
          buf->printf(",%lx", dw1);
          if (tag.words() > 2)
            buf->printf(",...");
        }
      buf->printf(")");
#undef TB_ENTRY
    }
}

class Tb_entry_ipc_fmt : public Tb_entry_formatter
{
public:
  Tb_entry_ipc_fmt() {}
  void print(String_buffer *, Tb_entry const *) const override {}
  Group_order has_partner(Tb_entry const *) const override
  { return Tb_entry::Group_order::first(); }
  Group_order is_pair(Tb_entry const *e, Tb_entry const *n) const override
  {
    if (static_cast<Tb_entry_ipc_res const *>(n)->pair_event() == e->number())
      return Group_order::last();
    else
      return Group_order::none();
  }
  Mword partner(Tb_entry const *) const override { return 0; }
};

class Tb_entry_ipc_res_fmt : public Tb_entry_formatter
{
public:
  Tb_entry_ipc_res_fmt() {}
  void print(String_buffer *, Tb_entry const *) const override {}
  Group_order has_partner(Tb_entry const *) const override
  { return Tb_entry::Group_order::last(); }
  Group_order is_pair(Tb_entry const *e, Tb_entry const *n) const override
  {
    if (static_cast<Tb_entry_ipc_res const *>(e)->pair_event() == n->number())
      return Group_order::first();
    else
      return Group_order::none();
  }
  Mword partner(Tb_entry const *e) const override
  { return static_cast<Tb_entry_ipc_res const *>(e)->pair_event(); }
};


// ipc / irq / shortcut success
static
void
formatter_ipc(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_ipc *e = static_cast<Tb_entry_ipc*>(tb);
  unsigned char type = e->ipc_type();

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

      buf->printf("]");
      print_words(buf, e->tag(), e->dword(0), e->dword(1));
    }

  buf->printf(" TO=");
  L4_timeout_pair to = e->timeout();
  if (type & L4_obj_ref::Ipc_send)
    {
      if (to.snd.is_absolute())
        {
          // absolute send timeout
          if (e->timeout_abs_snd_stored())
            {
              buf->printf("abs=%llu,rel", e->timeout_abs_snd());
              format_timeout(buf, e->timeout_abs_snd() - e->kclock(), true);
            }
          else
            buf->printf("abs-N/A");
        }
      else
        {
          // relative send timeout
          if (to.snd.is_never())
            buf->printf("INF");
          else if (to.snd.is_zero())
            buf->printf("0");
          else
            format_timeout(buf, to.snd.microsecs_rel(0), false);
        }
    }
  if (type & L4_obj_ref::Ipc_send && type & L4_obj_ref::Ipc_recv)
    buf->printf("/");
  if (type & L4_obj_ref::Ipc_recv)
    {
      if (to.rcv.is_absolute())
        {
          if (e->timeout_abs_rcv_stored())
            {
              // absolute receive timeout
              buf->printf("abs=%llu,rel", e->timeout_abs_rcv());
              format_timeout(buf, e->timeout_abs_rcv() - e->kclock(), true);
            }
          else
            buf->printf("abs-N/A");
        }
      else
        {
          // relative receive timeout
          if (to.rcv.is_never())
            buf->printf("INF");
          else if (to.rcv.is_zero())
            buf->printf("0");
          else
            format_timeout(buf, to.rcv.microsecs_rel(0), false);
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

  buf->printf("     %s%-*s %s [",
      e->is_np() ? "[np] " : "", tidlen, tidstr, m);
  if (e->tag().has_error())
    buf->printf("E");
  else
    print_msgtag(buf, e->tag());
  buf->printf("] L=%lx err=%lx (%s%s) (",
      e->from(), error.raw(), error.str_error(),
      error.ok() ? "" : error.snd_phase() ? "/snd" : "/rcv");
  if (e->ipc_has_recv_phase() && !e->tag().has_error() && e->tag().words() > 0)
    {
      buf->printf("%lx", e->dword(0));
      if (e->tag().words() > 1)
        {
          buf->printf(",%lx", e->dword(1));
          if (e->tag().words() > 2)
            buf->printf(",...");
        }
    }
  buf->printf(")");
}


// pagefault
static
void
formatter_pf(String_buffer *buf, Tb_entry *tb, const char *tidstr, int tidlen)
{
  Tb_entry_pf *e = static_cast<Tb_entry_pf*>(tb);
  Mword mw = PF::addr_to_msgword0(e->pfa(), e->error());
  char cause = (mw & 4) ? 'X' : (mw & 2) ? 'W' : 'R';
  buf->printf("pf:  %-*s pfa=" L4_PTR_FMT " ip=" L4_PTR_FMT " (%c%c) spc=%p (DID=",
      tidlen, tidstr, e->pfa(), e->ip(),
      PF::is_usermode_error(e->error()) ? tolower(cause) : cause,
      PF::is_translation_error(
        e->error()) ? '-' : 'p', static_cast<void *>(e->space()));
  Task *task = static_cast<Task*>(e->space());
  if (Kobject_dbg::is_kobj(task))
    buf->printf("%lx", task->dbg_info()->dbg_id());
  else
    buf->printf("???");
  buf->printf(") err=%lx", e->error());
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
    buf->printf("#%02x: err=%08x @ " L4_PTR_FMT, trapno(), error(), ip());
  else
    buf->printf(trapno() == 14
                  ? "#%02x: err=%04x @ " L4_PTR_FMT
                    " cs=%04x sp=" L4_PTR_FMT " cr2=" L4_PTR_FMT
                  : "#%02x: err=%04x @ " L4_PTR_FMT
                    " cs=%04x sp=" L4_PTR_FMT " eax=" L4_PTR_FMT,
                trapno(), error(), ip(), cs(), sp(),
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
