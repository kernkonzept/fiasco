INTERFACE:

#include "l4_types.h"

class Thread;


//-----------------------------------------------------------------
IMPLEMENTATION:

#include <cstdio>
#include <cstring>

#include "entry_frame.h"
#include "jdb.h"
#include "jdb_disasm.h"
#include "jdb_handler_queue.h"
#include "jdb_input.h"
#include "jdb_kobject.h"
#include "jdb_kobject_names.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_thread.h"
#include "jdb_util.h"
#include "kernel_console.h"
#include "kernel_task.h"
#include "keycodes.h"
#include "l4_types.h"
#include "push_console.h"
#include "simpleio.h"
#include "static_init.h"
#include "task.h"
#include "thread_object.h"
#include "thread_state.h"
#include "types.h"

IMPLEMENTATION [32bit]:
#define ADDR_FMT "%08lx"

IMPLEMENTATION [64bit]:
#define ADDR_FMT "%016lx"


IMPLEMENTATION:

class Jdb_tcb_ptr
{
public:
  Jdb_tcb_ptr(Address addr = 0)
    : _base(addr & ~(Context::Size-1)),
      _offs(addr &  (Context::Size-1))
  {}

  Jdb_tcb_ptr(Jdb_tcb_ptr const &p)
    : _base(p.base()), _offs(p.offs())
  {}

  inline bool valid() const
  { return _offs <= Context::Size-sizeof(Mword); }

  bool operator > (int offs) const
  {
    return offs < 0 ? _offs > Context::Size + offs*sizeof(Mword)
                    : _offs > offs*sizeof(Mword);
  }

  Jdb_tcb_ptr &operator += (int offs)
  { _offs += offs*sizeof(Mword); return *this; }

  inline Address addr() const
  { return _base + _offs; }

  inline Mword value() const
  { return *(Mword*)(_base + _offs); }

  inline void value(Mword v)
  { *(Mword*)(_base + _offs) = v; }

  inline bool is_user_value() const;

  inline const char *user_value_desc() const;

  inline Mword top_value(int offs) const
  { return *((Mword*)(Cpu::stack_align(_base + Context::Size)) + offs); }

  inline Address base() const
  { return _base; }

  inline Address offs() const
  { return _offs; }

  inline void offs(Address offs)
  { _offs = offs; }

  inline bool is_kern_code() const
  { return (Address)&Mem_layout::image_start <= value()
           && value() <= (Address)&Mem_layout::ecode;  };

  inline bool is_kobject() const
  { return Kobject_dbg::is_kobj(reinterpret_cast<void *>(value())); }

private:
  Address  _base;
  Address  _offs;
};

class Jdb_disasm_view
{
public:
  unsigned _x, _y;
};


class Jdb_stack_view
{
public:
  bool is_current;
  Jdb_entry_frame *ef;
  Jdb_tcb_ptr current;
  unsigned start_y;
  Address absy;
  Address addy, addx;
  bool memdump_is_colored;

  bool edit_registers();
};


class Jdb_tcb : public Jdb_module, public Jdb_kobject_handler
{
  static Kobject *threadid;
  static Address address;
  static char    first_char;
  static char    auto_tcb;

private:
  static void print_return_frame_regs(Jdb_tcb_ptr const &current, Mword ksp);
  static void print_entry_frame_regs(Thread *t);
  static void info_thread_state(Thread *t);

  static Jdb_disasm_view _disasm_view;
  static Jdb_stack_view  _stack_view;
};



Kobject *Jdb_tcb::threadid;
Address Jdb_tcb::address;
char    Jdb_tcb::first_char;
char    Jdb_tcb::auto_tcb;
Jdb_disasm_view Jdb_tcb::_disasm_view(Jdb_tcb::Disasm_x, Jdb_tcb::Disasm_y);
Jdb_stack_view  Jdb_tcb::_stack_view (Jdb_tcb::Stack_y);


// available from jdb_dump module
extern int jdb_dump_addr_task (Address addr, Space *task, int level)
  __attribute__((weak));


// default implementations: --------------------------------------------

// nothing special to do for edit registers
IMPLEMENT_DEFAULT bool Jdb_stack_view::edit_registers() { return true; }


PUBLIC
Jdb_stack_view::Jdb_stack_view(unsigned y)
: start_y(y), absy(0), memdump_is_colored(true)
{}

PUBLIC static inline
Mword
Jdb_stack_view::cols()
{
  return Jdb_screen::cols() - 1;
}

PUBLIC static inline
Mword
Jdb_stack_view::bytes_per_line()
{ return cols() * sizeof(Mword); }

PUBLIC
void
Jdb_stack_view::init(Address ksp, Jdb_entry_frame *_ef, bool _is_current)
{
  current = Jdb_tcb_ptr(ksp);

  absy = current.offs() / bytes_per_line();
  addx = (current.offs() % bytes_per_line()) / sizeof(Mword);
  addy = 0;
  ef = _ef;
  is_current = _is_current;
}

PUBLIC
void
Jdb_stack_view::print_value(Jdb_tcb_ptr const &p, bool highl = false)
{
  if (!p.valid() || !Jdb_util::is_mapped((void const*)p.addr()))
    {
      printf(" %.*s", (int)Jdb_screen::Mword_size_bmode, Jdb_screen::Mword_not_mapped);
      return;
    }

  const char *s1="", *s2="";
  if (highl)
    {
      s1 = Jdb::esc_emph;
      s2 = JDB_ANSI_END;
    }
  else if (p.is_user_value())
    {
      s1 = Jdb::esc_iret;
      s2 = JDB_ANSI_END;
    }
  else if (memdump_is_colored)
    {
      if (p.is_kern_code())
        {
          s1 = JDB_ANSI_COLOR(lightblue);
          s2 = JDB_ANSI_END;
        }
      else if (p.is_kobject())
        {
          s1 = JDB_ANSI_COLOR(lightgreen);
          s2 = JDB_ANSI_END;
        }
      /* else if (p.in_backtrace(...
         {
         s1 = Jdb::esc_bt;
         s2 = "\033[m";
         } */
    }

  printf(" %s" ADDR_FMT "%s", s1, p.value(), s2);
}


PUBLIC
void
Jdb_stack_view::dump(bool dump_only)
{
  Jdb_tcb_ptr p;
  unsigned ylen;

  if (dump_only)
    {
      p = Jdb_tcb_ptr(current.base());
      ylen = Context::Size / bytes_per_line();
      puts("");
    }
  else
    {
      p = current;
      p.offs(absy * bytes_per_line());
      Jdb::cursor(start_y, 1);
      ylen = Jdb_screen::height() - start_y;
    }

  for (unsigned y = 0; y < ylen; ++y)
    {
      Kconsole::console()->getchar_chance();

      if (p.valid())
        {
          printf("    %03lx ", p.addr() & 0xfff);
          for (unsigned x = 0; x < cols(); ++x, p+=1)
            print_value(p);
          putchar('\n');
        }
      else
        Jdb::clear_to_eol();
    }
}

PRIVATE inline
unsigned
Jdb_stack_view::posx()
{ return addx * (Jdb_screen::Mword_size_bmode + 1) + 9; }

PRIVATE inline
unsigned
Jdb_stack_view::posy()
{ return addy + start_y; }

PUBLIC
void
Jdb_stack_view::highlight(bool highl)
{
  current.offs(absy*bytes_per_line() + addy*bytes_per_line() + addx * sizeof(Mword));
  Jdb_tcb_ptr first_col = current;
  first_col.offs(absy*bytes_per_line() + addy*bytes_per_line());

  if (!current.valid())
    return;

  Jdb::cursor(posy(), 1);
  if (highl)
    printf("%08lx", current.addr() & 0xffffffff);
  else
    printf("    %03lx ", first_col.addr() & 0xfff);
  Jdb::cursor(posy(), posx());
  print_value(current, highl);

  String_buf<120> kobj_desc;
  Kobject_dbg::Iterator o;

  if (current.is_kern_code())
    kobj_desc.printf("Kernel code"); // todo: print kernel function name
  else if (current.is_user_value())
    kobj_desc.printf("Return frame: %s", current.user_value_desc());
  else if ((o = Kobject_dbg::pointer_to_obj(reinterpret_cast<void *>(current.value()))) != Kobject_dbg::end())
    Jdb_kobject::obj_description(&kobj_desc, true, *o);

  Jdb::printf_statline("tcb", "<CR>=dump <Space>=Disas",
                       "%s", kobj_desc.c_str());
}

PUBLIC
bool
Jdb_stack_view::handle_key(int key, bool *redraw)
{
  Mword   lines    = Jdb_screen::height() - start_y;
  Mword   max_lines = (Context::Size + bytes_per_line() - 1)/bytes_per_line();
  Address max_absy = max_lines - lines;

  if (lines > max_lines)
    max_absy = 0;

  if (lines > max_lines - absy)
    lines = max_lines - absy;

  if (key == 'e')
    edit_stack(redraw);
  else
    return Jdb::std_cursor_key(key, this->cols(), lines, max_absy,
      &absy, &addy, &addx, redraw);

  return true;
}

PUBLIC
void
Jdb_stack_view::edit_stack(bool *redraw)
{
  if (current.valid())
    {
      Mword value;
      int c;

      Jdb::cursor(posy(), posx());
      printf(" %.*s", (int)Jdb_screen::Mword_size_bmode, Jdb_screen::Mword_blank);
      Jdb::printf_statline("tcb",
          is_current ? "<Space>=edit registers" : 0,
          "edit <" ADDR_FMT "> = " ADDR_FMT,
          current.addr(), current.value());

      Jdb::cursor(posy(), posx() + 1);
      c = Jdb_core::getchar();

      if (c==KEY_ESC)
        {
          *redraw = true;
          return;
        }

      if (c != ' ' || !is_current)
        {
          // edit memory
          putchar(c);
          Jdb::printf_statline("tcb", 0, "edit <" ADDR_FMT "> = " ADDR_FMT,
              current.addr(), current.value());
          Jdb::cursor(posy(), posx() + 1);
          if (!Jdb_input::get_mword(&value, c, 16))
            {
              Jdb::cursor(posy(), posx());
              print_value(current);
              return;
            }
          else
            current.value(value);
        }
      else
        {
          // edit registers
          Jdb::cursor(posy(), posx());
          print_value(current);
          edit_registers();
          return;
        }
      *redraw = true;
    }
}

PUBLIC
Jdb_disasm_view::Jdb_disasm_view(unsigned x, unsigned y)
: _x(x), _y(y)
{}

PUBLIC
void
Jdb_disasm_view::show(Jdb_tcb_ptr const &p, Space *s, bool dump_only)
{
  if (!Jdb_disasm::avail())
    return;

  Address disass_addr = p.top_value(-5);
  if (dump_only)
    {
      for (unsigned i = 0; i < 20; ++i)
        Jdb_disasm::show_disasm_line(Jdb_screen::width(), disass_addr, 0, s);
      return;
    }

  Jdb::cursor(_y, _x);
  putstr(Jdb::esc_emph);
  Jdb_disasm::show_disasm_line(-40, disass_addr, 0, s);
  putstr("\033[m");
  Jdb::cursor(_y + 1, _x);
  Jdb_disasm::show_disasm_line(-40, disass_addr, 0, s);
}


PUBLIC
Jdb_tcb::Jdb_tcb()
  : Jdb_module("INFO"), Jdb_kobject_handler((Thread*)0)
{
  static Jdb_handler enter(at_jdb_enter);

  Jdb::jdb_enter.add(&enter);
  Jdb_kobject::module()->register_handler(this);
}

static void
Jdb_tcb::at_jdb_enter()
{
  if (auto_tcb)
    {
      // clear any keystrokes in queue
      Jdb::set_next_cmd(0);
      Jdb::push_cons()->push('t');
      Jdb::push_cons()->push(' ');
    }
}


PUBLIC virtual
Kobject *
Jdb_tcb::parent(Kobject_common *o)
{
  Thread *t = cxx::dyn_cast<Thread*>(o);
  if (!t)
    return 0;

  return static_cast<Task*>(t->space());
}

PRIVATE static inline
char *
Jdb_tcb::vcpu_state_str(Mword state, char *s, int len)
{
  snprintf(s, len, "%c%c%c%c%c%c",
           (state & Vcpu_state::F_fpu_enabled) ? 'F' : 'f',
           (state & Vcpu_state::F_user_mode)   ? 'U' : 'u',
           (state & Vcpu_state::F_debug_exc)   ? 'D' : 'd',
           (state & Vcpu_state::F_exceptions)  ? 'E' : 'e',
           (state & Vcpu_state::F_page_faults) ? 'P' : 'p',
           (state & Vcpu_state::F_irqs)        ? 'I' : 'i');
  s[len - 1] = 0;
  return s;
}

PUBLIC static
Jdb_module::Action_code
Jdb_tcb::show(Thread *t, int level, bool dump_only)
{
  Thread *t_current      = Jdb::get_current_active();
  bool is_current_thread;
  bool redraw_screen     = !dump_only;
  Jdb_entry_frame *ef    = Jdb::get_entry_frame(Jdb::current_cpu);
#if 0
  Address bt_start       = 0;
#endif

  if (!t && !t_current)
    return NOTHING;

  if (!t)
    t = t_current;

  is_current_thread = t == t_current;

  if (level == 0)
    {
      Jdb::clear_screen(Jdb::FANCY);
      redraw_screen = false;
    }

whole_screen:

  if (redraw_screen)
    {
      Jdb::clear_screen(Jdb::NOFANCY);
      redraw_screen = false;
    }

  String_buf<12> time_str;

  putstr("thread  : ");
  Jdb_kobject::print_uid(t, 3);
  print_thread_uid_raw(t);
  printf("\tCPU: %u:%u ", cxx::int_value<Cpu_number>(t->home_cpu()),
                          cxx::int_value<Cpu_number>(t->get_current_cpu()));

  printf("\tprio: %02x\n",
         (unsigned)t->sched()->prio());

  printf("state   : %03lx ", t->state(false));
  Jdb_thread::print_state_long(t);

  putstr("\nwait for: ");
  if (!t->partner())
    putstr("---  ");
  else
    Jdb_thread::print_partner(t, 4);

  putstr("\tpolling: ");
  Jdb_thread::print_snd_partner(t, 3);

  putstr("\trcv descr: ");

  if ((t->state(false) & Thread_ipc_mask) == Thread_receive_wait
      && t->rcv_regs())
    printf("%08lx", t->rcv_regs()->from_spec());
  else
    putstr("        ");

  putstr("\n"
         "lcked by: ");
  //if (t->thread_lock()->lock_owner())
  //  Jdb_kobject::print_uid(static_cast<Thread*>(t->thread_lock()->lock_owner()), 3);

  putstr("\t\t\ttimeout  : ");
  if (t->_timeout && t->_timeout->is_set())
    {
      Signed64 diff = (t->_timeout->get_timeout(Kip::k()->clock)) * 1000;
      if (diff < 0)
        time_str.printf("over");
      else
        Jdb::write_ll_ns(&time_str, diff, false);

      time_str.terminate();
      printf("%-13s", time_str.begin());
    }

  time_str.reset();
  putstr("\ncpu time: ");
  Jdb::write_ll_ns(&time_str, t->consumed_time()*1000, false);
  time_str.terminate();
  printf("%-13s", time_str.begin());

  printf("\t\ttimeslice: %llu/%llu %cs\n"
         "pager\t: ",
         t->sched()->left(), ~0ULL/*t->sched()->quantum()*/, Config::char_micro);
  print_kobject(t, t->_pager.raw());

  putstr("\ttask     : ");
  if (t->space() == Kernel_task::kernel_task())
    putstr(" kernel        ");
  else
    print_kobject(static_cast<Task*>(t->space()));

  putstr("\nexc-hndl: ");
  print_kobject(t, t->_exc_handler.raw());

  printf("\tUTCB     : %08lx/%08lx",
         (Mword)t->utcb().kern(), (Mword)t->utcb().usr().get());

#if 0
  putstr("\tready  lnk: ");
  if (t->state(false) & Thread_ready)
    {
      if (t->_ready_next)
        Jdb_kobject::print_uid(Thread::lookup(t->_ready_next), 3);
      else if (is_current_thread)
        putstr(" ???.??");
      else
        putstr("\033[31;1m???.??\033[m");
      if (t->_ready_prev)
        Jdb_kobject::print_uid(Thread::lookup(t->_ready_prev), 4);
      else if (is_current_thread)
        putstr(" ???.??");
      else
        putstr(" \033[31;1m???.??\033[m");
      putchar('\n');
    }
  else
    puts("--- ---");
#endif

  putchar('\n');

  putstr("vCPU    : ");
  if (t->state(false) & Thread_vcpu_enabled)
    {
      char st1[7];
      char st2[7];
      Vcpu_state *v = t->vcpu_state().kern();
      printf("%08lx/%08lx S=", (Mword)v, (Mword)t->vcpu_state().usr().get());
      print_kobject(static_cast<Task*>(t->vcpu_user_space()));
      putchar('\n');
      printf("vCPU    : c=%s s=%s sf=%c e-ip=%08lx e-sp=%08lx\n",
             vcpu_state_str(v->state, st1, sizeof(st1)),
             vcpu_state_str(v->_saved_state, st2, sizeof(st2)),
             (v->sticky_flags & Vcpu_state::Sf_irq_pending) ? 'P' : '-',
             v->_entry_ip, v->_entry_sp);
    }
  else
    putstr("---\nvCPU    : ---\n");

  Address ksp  = is_current_thread ? ef->ksp()
                                   : (Address)t->get_kernel_sp();

#if 0
  Address tcb  = (Address)context_of((void*)ksp);
#endif
  _stack_view.init(ksp, ef, is_current_thread);

  if (is_current_thread)
    print_entry_frame_regs(t);

  else if (t->space() != Kernel_task::kernel_task())
    {
      if (!dump_only)
        Jdb::cursor(11, 1);
      info_thread_state(t);
      putchar('\n');
      print_return_frame_regs(_stack_view.current, ksp);

      _disasm_view.show(_stack_view.current, t->space(), dump_only);
    }
  else
    {
      // kernel thread
      if (!dump_only)
        Jdb::cursor(15, 1);
      printf("kernel SP=" ADDR_FMT, ksp);
    }

dump_stack:

  // dump the stack from ksp bottom right to tcb_top
  _stack_view.dump(dump_only);

  if (dump_only)
    return NOTHING;

  for (bool redraw=false; ; )
    {
      _stack_view.highlight(true);
      int c=Jdb_core::getchar();
      _stack_view.highlight(false);
      Jdb::cursor(Jdb_screen::height(), 6);

      if (c == KEY_CURSOR_HOME && level > 0)
        return GO_BACK;

      if (!_stack_view.handle_key(c, &redraw))
        {
          switch (c)
            {
            case KEY_RETURN:
            case KEY_RETURN_2:
              if (jdb_dump_addr_task && _stack_view.current.valid())
                {
                  if (!jdb_dump_addr_task(_stack_view.current.value(),
                        _stack_view.current.is_user_value() ? t->space() : 0, level+1))
                    return NOTHING;
                  redraw_screen = true;
                }
              break;
            case KEY_TAB:
              //bt_start = search_bt_start(tcb, (Mword*)ksp, is_current_thread);
              redraw = true;
              break;
            case ' ':
              if (Jdb_disasm::avail() && _stack_view.current.valid())
                {
                  printf("V %lx\n", _stack_view.current.value());
                  if (!Jdb_disasm::show(_stack_view.current.value(),
                        _stack_view.current.is_user_value() ? t->space() : 0, level+1, true))
                    return NOTHING;
                  redraw_screen = true;
                }
              break;
            case 'u':
              if (Jdb_disasm::avail() && _stack_view.current.valid())
                {
                  Jdb::printf_statline("tcb", "<CR>=disassemble here",
                                        "u[address=%08lx task=%lx] ",
                                        _stack_view.current.value(),
                                        Kobject_dbg::pointer_to_id(t->space()));
                  int c1 = Jdb_core::getchar();
                  if ((c1 != KEY_RETURN) && c1 != KEY_RETURN_2 && (c1 != ' '))
                    {
                      Jdb::printf_statline("tcb", 0, "u");
                      Jdb::execute_command("u", c1);
                      return NOTHING;
                    }

                  if (!Jdb_disasm::show(_stack_view.current.value(),
                        _stack_view.current.is_user_value() ? t->space() : 0, level+1, true))
                    return NOTHING;
                  redraw_screen = true;
                }
              break;
#if 0
            case 'r': // ready-list
              putstr("[n]ext/[p]revious in ready list?");
              switch (Jdb_core::getchar())
                {
                case 'n':
                  if (t->_ready_next)
                    {
                      t = static_cast<Thread*>(t->_ready_next);
                      goto new_tcb;
                    }
                  break;
                case 'p':
                  if (t->_ready_prev)
                    {
                      t = static_cast<Thread*>(t->_ready_prev);
                      goto new_tcb;
                    }
                  break;
                }
              break;
#endif
            case 'C':
              _stack_view.memdump_is_colored = !_stack_view.memdump_is_colored;
              redraw = true;
              break;
            case KEY_ESC:
              Jdb::abort_command();
              return NOTHING;
            default:
              if (Jdb::is_toplevel_cmd(c))
                return NOTHING;
              break;
            }
        }
      if (redraw_screen)
        goto whole_screen;
      if (redraw)
        goto dump_stack;
    }
} 

/* --- original L4 screen ------------------------------------------------------
thread: 0081 (001.01) <00020401 00080000>                               prio: 10
state : 85, ready                            lists: 81                   mcp: ff

wait for: --                             rcv descr: 00000000   partner: 00000000
sndq    : 0081 0081                       timeouts: 00000000   waddr0/1: 000/000
cpu time: 0000000000 timeslice: 01/0a

pager   : --                            prsent lnk: 0080 0080
ipreempt: --                            ready link : 0080 0080
xpreempt: --
                                        soon wakeup lnk: 
EAX=00202dfe  ESI=00020401  DS=0008     late wakeup lnk: 
EBX=00000028  EDI=00080000  ES=0008
ECX=00000003  EBP=e0020400
EDX=00000001  ESP=e00207b4

700:
720:
740:
760:
780:
7a0:                                                  0000897b 00000020 00240082
7c0:  00000000 00000000 00000000 00000000    00000000 00000000 00000000 00000000
7e0:  00000000 00000000 ffffff80 00000000    0000001b 00003200 00000000 00000013
L4KD: 
------------------------------------------------------------------------------*/

PUBLIC
Jdb_module::Action_code
Jdb_tcb::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  static Address tcb_addr = 0;
  if (cmd == 0)
    {
      if (args == &first_char)
        {
          switch (first_char)
            {
              case '+':
              case '-':
                printf("%c\n", first_char);
                auto_tcb = first_char == '+';
                break;
              case '?':
                args      = &address;
                fmt       = " addr=" ADDR_FMT " => ";
                putchar(first_char);
                return Jdb_module::EXTRA_INPUT;
              case 'a':
                args      = &tcb_addr;
                fmt       = " tcb=%x => ";
                putchar(first_char);
                return Jdb_module::EXTRA_INPUT;
              case KEY_RETURN:
              case KEY_RETURN_2:
                show(0, 0, false);
                return NOTHING;
              default:
                args      = &threadid;
                fmt       = "%q";
                next_char = first_char;
                return Jdb_module::EXTRA_INPUT_WITH_NEXTCHAR;
            }
        }
      else if (args == &address)
        {
          address &= ~(Context::Size-1);
          Jdb_kobject::print_uid(reinterpret_cast<Thread*>(address), 3);
          putchar('\n');
        }
      else if (args == &tcb_addr)
        show((Thread*)tcb_addr, 0, false);
      else
        {
          Thread *t = cxx::dyn_cast<Thread *>(threadid);
          if (t)
            show(t, 0, false);
          else
            printf("\nNot a thread\n");
        }
    }
  else if (cmd == 1)
    {
      if (args == &first_char)
        {
          args      = &threadid;
          fmt       = "%q";
          next_char = first_char;
          return Jdb_module::EXTRA_INPUT_WITH_NEXTCHAR;
        }
      else if (args == &threadid)
        {
          Thread *t = cxx::dyn_cast<Thread *>(threadid);
          if (t)
            show(t, 1, true);
        }
    }

  return NOTHING;
}

PUBLIC
Kobject_common *
Jdb_tcb::follow_link(Kobject_common *o)
{
  Thread *t = cxx::dyn_cast<Thread *>(Kobject::from_dbg(o->dbg_info()));
  if (t->space() == Kernel_task::kernel_task())
    return o;
  return static_cast<Kobject*>(static_cast<Task*>(t->space()));
}

PUBLIC
bool
Jdb_tcb::show_kobject(Kobject_common *o, int level)
{
  Thread *t = cxx::dyn_cast<Thread *>(Kobject::from_dbg(o->dbg_info()));
  return show(t, level, false);
}

PRIVATE static
bool
Jdb_tcb::is_current(Thread *t)
{
  return t == Jdb::get_thread(t->get_current_cpu());
}

PRIVATE
void
Jdb_tcb::print_cpu(String_buffer *buf, Thread *t)
{
  buf->printf(" C=%u", cxx::int_value<Cpu_number>(t->home_cpu()));
  if (t->home_cpu() != t->get_current_cpu())
    buf->printf(":%u", cxx::int_value<Cpu_number>(t->get_current_cpu()));
}

PUBLIC
void
Jdb_tcb::show_kobject_short(String_buffer *buf, Kobject_common *o)
{
  Thread *t = cxx::dyn_cast<Thread *>(Kobject::from_dbg(o->dbg_info()));
  bool is_current = Jdb_tcb::is_current(t);
  if (t == Context::kernel_context(t->home_cpu()))
    {
      buf->printf(" {KERNEL}");
      print_cpu(buf, t);
    }

  if (t->space() == Kernel_task::kernel_task())
    buf->printf(" R=%ld rdy %s", t->ref_cnt(),
                is_current ? " " JDB_ANSI_COLOR(green) "cur" JDB_ANSI_END : "");
  else
    {
      print_cpu(buf, t);

      buf->printf(" S=D:%lx R=%ld%s%s",
                  Kobject_dbg::pointer_to_id(t->space()),
                  t->ref_cnt(),
                  t->in_ready_list() ? " rdy" : "",
                  is_current ? " " JDB_ANSI_COLOR(green) "cur" JDB_ANSI_END : "");
    }
}

PUBLIC
Jdb_module::Cmd const *
Jdb_tcb::cmds() const
{
  static Cmd cs[] =
    {
        { 0, "t", "tcb", "%C",
          "t[<threadid>]\tshow current/given thread control block (TCB)\n"
          "t{+|-}\tshow current thread control block at every JDB entry\n",
          &first_char },
        { 1, "", "tcbdump", "%C", 0, &first_char },
    };
  return cs;
}

PUBLIC
int
Jdb_tcb::num_cmds() const
{ return 2; }

static Jdb_tcb jdb_tcb INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

int
jdb_show_tcb(Thread *t, int level)
{ return Jdb_tcb::show(t, level, false); }

static inline
void
Jdb_tcb::print_thread_uid_raw(Thread *t)
{
  printf(" <%p> ", t);
}

PRIVATE static
void
Jdb_tcb::print_kobject(Cap_index n)
{
  printf("[C:%4lx]       ", cxx::int_value<Cap_index>(n));
}

PRIVATE static
void
Jdb_tcb::print_kobject(Kobject *o)
{
  printf("D:%4lx         ", o ? o->dbg_info()->dbg_id() : 0);
}

PRIVATE static
void
Jdb_tcb::print_kobject(Thread *t, Cap_index capidx)
{
  Space *space = t->space();
  if (!space || space->obj_map_max_address() <= capidx)
    {
      print_kobject(capidx);
      return;
    }

  Obj_space::Capability *c = space->jdb_lookup_cap(capidx);
  if (!c || !c->valid())
    {
      print_kobject(capidx);
      return;
    }

  if (Kobject_dbg::pointer_to_obj(c->obj()) == Kobject_dbg::end())
    {
      printf("[C:%4lx] NOB: %p\n", cxx::int_value<Cap_index>(capidx), c->obj());
      return;
    }

  printf("[C:%4lx] D:%4lx", cxx::int_value<Cap_index>(capidx),
         c->obj()->dbg_info()->dbg_id());
}

//
//-----------------------------------------------------------------------------
// prompt extension for thread names
class Jdb_thread_name_ext : public Jdb_prompt_ext
{
public:
  void ext();
  void update();
};

IMPLEMENT
void
Jdb_thread_name_ext::ext()
{
  if (Jdb::get_current_active())
    {
      Jdb_kobject_name *nx = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(Jdb::get_current_active());
      if (nx && nx->name()[0])
        printf("[%*.*s] ", nx->max_len(), nx->max_len(), nx->name());
    }
}

IMPLEMENT
void
Jdb_thread_name_ext::update()
{
  Jdb::get_current(Jdb::current_cpu);
}

static Jdb_thread_name_ext jdb_thread_name_ext INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

