INTERFACE:

#include <cxx/function>
#include <cxx/type_traits>

#include "l4_types.h"
#include "cpu_mask.h"
#include "jdb_types.h"
#include "jdb_core.h"
#include "jdb_handler_queue.h"
#include "mem.h"
#include "per_cpu_data.h"
#include "processor.h"
#include "string_buffer.h"
#include "trap_state.h"

class Context;
class Space;
class Thread;
class Push_console;

class Jdb_entry_frame;

class Jdb : public Jdb_core
{
public:
  struct Remote_func : cxx::functor<void (Cpu_number)>
  {
    bool running;

    Remote_func() = default;
    Remote_func(Remote_func const &) = delete;
    Remote_func operator = (Remote_func const &) = delete;

    void reset_mp_safe()
    {
      set_monitored_address(&_f, Func{nullptr});
    }

    void set_mp_safe(cxx::functor<void (Cpu_number)> const &rf)
    {
      Remote_func const &f = static_cast<Remote_func const &>(rf);
      running = true;
      _d = f._d;
      Mem::mp_mb();
      set_monitored_address(&_f, f._f);
      Mem::mp_mb();
    }

    void monitor_exec(Cpu_number current_cpu)
    {
      if (Func f = monitor_address(current_cpu, &_f))
        {
          _f = nullptr;
          f(_d, cxx::forward<Cpu_number>(current_cpu));
          Mem::mp_mb();
          running = false;
        }
    }

    void wait() const
    {
      for (;;)
        {
          Mem::mp_mb();
          if (!running)
            break;
          Proc::pause();
        }
    }
  };

  static Per_cpu<Jdb_entry_frame*> entry_frame;
  static Cpu_number triggered_on_cpu;
  static Per_cpu<Remote_func> remote_func;

  static void write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign);
  static void write_tsc(String_buffer *buf, Signed64 tsc, bool sign);

  static int FIASCO_FASTCALL enter_jdb(Trap_state *ts, Cpu_number cpu);
  static void cursor_end_of_screen();
  static void cursor_home();
  static void printf_statline(const char *prompt, const char *help,
                              const char *format, ...)
  __attribute__((format(printf, 3, 4)));
  static void save_disable_irqs(Cpu_number cpu);
  static void restore_irqs(Cpu_number cpu);
  static void store_system_clock_on_enter();
  static void clear_system_clock_on_enter();
  static Unsigned64 system_clock_on_enter();

protected:
  template< typename T >
  static void set_monitored_address(T *dest, T val);

  template< typename T >
  static T monitor_address(Cpu_number, T const volatile *addr);

private:
  Jdb();			// default constructors are undefined
  Jdb(const Jdb&);

  static char hide_statline;
  static char next_cmd;
  static Per_cpu<String_buf<81> > error_buffer;
  static bool was_input_error;

  static const char *toplevel_cmds;
  static const char *non_interactive_cmds;

  // state for traps in JDB itself
  static Per_cpu<bool> in_jdb;
  static bool in_service;
  static bool leave_barrier;
  static unsigned long cpus_in_debugger;
  static bool never_break;
  static bool jdb_active;
  static Unsigned64 _system_clock_on_enter;

  static void enter_trap_handler(Cpu_number cpu);
  static void leave_trap_handler(Cpu_number cpu);
  static bool handle_conditional_breakpoint(Cpu_number cpu, Jdb_entry_frame *e);
  static bool handle_early_debug_traps(Jdb_entry_frame *e);
  static void handle_nested_trap(Jdb_entry_frame *e);
  static bool handle_user_request(Cpu_number cpu);
  static bool handle_debug_traps(Cpu_number cpu);
  static bool test_checksums();

public:
  static Jdb_handler_queue jdb_enter;
  static Jdb_handler_queue jdb_leave;

  // esc sequences for highlighting
  static char  esc_iret[];
  static char  esc_bt[];
  static char  esc_emph[];
  static char  esc_emph2[];
  static char  esc_mark[];
  static char  esc_line[];
  static char  esc_symbol[];

};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include <csetjmp>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <simpleio.h>

#include "config.h"
#include "delayloop.h"
#include "feature.h"
#include "jdb_core.h"
#include "jdb_entry_frame.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "processor.h"
#include "push_console.h"
#include "static_init.h"
#include "keycodes.h"
#include "libc_backend.h"
#include "kernel_uart.h"
#include "ipi.h"
#include "logdefs.h"
#include "task.h"
#include "timer.h"

KIP_KERNEL_FEATURE("jdb");

Jdb_handler_queue Jdb::jdb_enter;
Jdb_handler_queue Jdb::jdb_leave;

DEFINE_PER_CPU Per_cpu<String_buf<81> > Jdb::error_buffer;
char Jdb::next_cmd;			// next global command to execute

char Jdb::hide_statline;		// show status line on enter_kdebugger
DEFINE_PER_CPU Per_cpu<Jdb_entry_frame*> Jdb::entry_frame;
Cpu_number Jdb::triggered_on_cpu = Cpu_number::nil(); // first CPU entered JDB
bool Jdb::was_input_error;		// error in command sequence

DEFINE_PER_CPU Per_cpu<Jdb::Remote_func> Jdb::remote_func;

// holds all commands executable in top level (regardless of current mode)
const char *Jdb::toplevel_cmds = "j_";

// a short command must be included in this list to be enabled for non-
// interactive execution
const char *Jdb::non_interactive_cmds = "bEIJLMNOPSU^Z*";

DEFINE_PER_CPU Per_cpu<bool> Jdb::in_jdb;
bool Jdb::never_break;		// never enter JDB
bool Jdb::jdb_active;
bool Jdb::in_service;
bool Jdb::leave_barrier;
unsigned long Jdb::cpus_in_debugger;
Unsigned64 Jdb::_system_clock_on_enter;

IMPLEMENT_DEFAULT inline template< typename T >
void
Jdb::set_monitored_address(T *dest, T val)
{ *const_cast<T volatile *>(dest) = val; }

IMPLEMENT_DEFAULT inline template< typename T >
T
Jdb::monitor_address(Cpu_number, T const volatile *addr)
{ return *addr; }

IMPLEMENT_DEFAULT inline NEEDS["timer.h"]
void
Jdb::store_system_clock_on_enter()
{
  if (!_system_clock_on_enter)
    _system_clock_on_enter = Timer::system_clock();
}

IMPLEMENT_DEFAULT inline
void
Jdb::clear_system_clock_on_enter()
{
  _system_clock_on_enter = 0;
}

IMPLEMENT_DEFAULT inline
Unsigned64
Jdb::system_clock_on_enter()
{
  return _system_clock_on_enter;
}

IMPLEMENT_DEFAULT static
bool
Jdb::handle_early_debug_traps(Jdb_entry_frame *)
{
  return false;
}

PUBLIC static
bool
Jdb::cpu_in_jdb(Cpu_number cpu)
{ return Cpu::online(cpu) && in_jdb.cpu(cpu); }


PUBLIC static
template< typename Func >
void
Jdb::foreach_cpu(Func const &f)
{
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    if (cpu_in_jdb(i))
      f(i);
}

PUBLIC static
template< typename Func >
bool
Jdb::foreach_cpu(Func const &f, bool positive)
{
  bool r = positive;
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    if (cpu_in_jdb(i))
      {
        bool res = f(i);

        if (positive)
          r = r && res;
        else
          r = r || res;
      }

  return r;
}

PUBLIC static inline
void
Jdb::set_next_cmd(char cmd)
{ next_cmd = cmd; }

PUBLIC static inline
int
Jdb::get_next_cmd()
{ return next_cmd; }

/** Command aborted. If we are interpreting a debug command like
 *  enter_kdebugger("*#...") this is an error
 */
PUBLIC
static void
Jdb::abort_command()
{
  cursor(Jdb_screen::height(), 6);
  clear_to_eol();

  was_input_error = true;
}


// go to bottom of screen and print some text in the form "jdb: ..."
// if no text follows after the prompt, prefix the current thread number
IMPLEMENT
void
Jdb::printf_statline(const char *prompt, const char *help,
                     const char *format, ...)
{
  cursor(Jdb_screen::height(), 1);
  int w = Jdb_screen::width();
  prompt_start();
  if (prompt)
    {
      putstr(prompt);
      putstr(": ");
      w -= strlen(prompt) + 2;
    }
  else
    {
      Jdb::prompt();
      w -= Jdb::prompt_len();
    }
  prompt_end();
  // avoid -Wformat-zero-length warning
  if (format && (format[0] != '_' || format[1] != '\0'))
    {
      char buf[128];
      va_list list;
      va_start(list, format);
      vsnprintf(buf, sizeof(buf), format, list);
      va_end(list);
      putstr(buf);
      w -= print_len(buf);
    }
  if (help)
    {
      if (print_len(help) < w - 1)
        printf("%*.*s", w, w, help);
      else
        printf(" %*.*s...", w - 4, w - 4, help);
    }
  else
    clear_to_eol();
}

PUBLIC static
bool Jdb::is_toplevel_cmd(char c)
{
  char cm[] = {c, 0};
  Jdb_core::Cmd cmd = Jdb_core::has_cmd(cm);

  if (cmd.cmd || (nullptr != strchr(toplevel_cmds, c)))
    {
      set_next_cmd(c);
      return true;
    }

  return false;
}


PUBLIC static
int
Jdb::execute_command(const char *s, int first_char = -1)
{
  Jdb_core::Cmd cmd = Jdb_core::has_cmd(s);

  if (cmd.cmd)
    {
      const char *args = nullptr;
      if (!short_mode)
        {
          args = s + strlen(cmd.cmd->cmd);
          while (isspace(*args))
            ++args;
        }
      return Jdb_core::exec_cmd(cmd, args, first_char) == 2 ? 1 : 0;
    }

  return 0;
}

PRIVATE static
int
Jdb::execute_command_mode(bool is_short, const char *s, int first_char = -1)
{
  bool orig_mode = short_mode;
  short_mode = is_short;
  int r = execute_command(s, first_char);
  short_mode = orig_mode;
  return r;
}

PUBLIC static
int
Jdb::execute_command_short(const char *s, int first_char = -1)
{
  return execute_command_mode(true, s, first_char);
}

PUBLIC static
int
Jdb::execute_command_long(const char *s, int first_char = -1)
{
  return execute_command_mode(false, s, first_char);
}

PUBLIC static
Push_console *
Jdb::push_cons()
{
  static Push_console c;
  return &c;
}

// Interprete str as non interactive commands for Jdb. We allow mostly 
// non-interactive commands here (e.g. we don't allow d, t, l, u commands)
PRIVATE static
int
Jdb::execute_command_ni(char const *str, int len)
{
  for (; len && *str; ++str, --len)
    push_cons()->push(*str);

  push_cons()->push('_'); // terminating return

  bool leave = true;
  for (;;)
    {
      // Prevent output of sequences. Do this inside the loop because some
      // commands do Console::start_exclusive() + Console::end_exclusive().
      Kconsole::console()->change_state(0, 0, ~Console::OUTENABLED, 0);

      if (short_mode)
        {
          int c = getchar();
          if (c == KEY_RETURN || c == KEY_RETURN_2 || c == ' ')
            break;

          was_input_error = true;
          if (nullptr != strchr(non_interactive_cmds, c))
            {
              char _cmd[] = {static_cast<char>(c), 0};
              Jdb_core::Cmd cmd = Jdb_core::has_cmd(_cmd);

              if (cmd.cmd)
                {
                  if (Jdb_core::exec_cmd (cmd, nullptr) != 3)
                    was_input_error = false;
                }
            }

          if (was_input_error)
            {
              leave = false;
              break;
            }
        }
      else
        {
          Jdb_core::Cmd cmd(nullptr, nullptr);
          char const *args;
          input_long_mode(&cmd, &args);
          if (!cmd.cmd)
            break;

          if (Jdb_core::exec_cmd(cmd, args) == 3)
            {
              leave = false;
              break;
            }
        }
    }

  push_cons()->flush();
  // re-enable all consoles but GZIP
  Kconsole::console()->change_state(0, Console::GZIP, ~0UL, Console::OUTENABLED);
  return leave;
}

PRIVATE static
bool
Jdb::input_short_mode(Jdb::Cmd *cmd, char const **args, int &cmd_key)
{
  *args = nullptr;
  for (;;)
    {
      int c;
      do
	{
	  if ((c = get_next_cmd()))
	    set_next_cmd(0);
	  else
	    c = getchar();
	}
      while (c < ' ' && c != KEY_RETURN && c != KEY_RETURN_2);

      if (c == KEY_F1)
	c = 'h';

      if (c >= 0x100) // see keycodes.h: no special keys on command line
        continue;

      printf("\033[K%c", c); // clreol + print key

      char cmd_buffer[2] = { static_cast<char>(c), 0 };

      *cmd = Jdb_core::has_cmd(cmd_buffer);
      if (cmd->cmd)
	{
	  cmd_key = c;
	  return false; // do not leave the debugger
	}
      else if (!handle_special_cmds(c))
	return true; // special command triggered a JDB leave
      else if (c == KEY_RETURN || c == KEY_RETURN_2)
	{
	  hide_statline = false;
	  cmd_key = c;
	  return false;
	}
      else
        {
          printf(" -- unknown command\n");
          return false;
        }
    }
}


class Cmd_buffer
{
private:
  unsigned  _l;
  char _b[256];

public:
  Cmd_buffer() {}
  char *buffer() { return _b; }
  int len() const { return _l; }
  void flush() { _l = 0; _b[0] = 0; }
  void cut(int l) 
  {
    if (l < 0)
      l = _l + l;

    if (l >= 0 && static_cast<unsigned>(l) < _l)
      {
	_l = l;
	_b[l] = 0;
      }
  }

  void append(int c) { if (_l + 1 < sizeof(_b)) { _b[_l++] = c; _b[_l] = 0; } }
  void append(char const *str, int len)
  {
    if (_l + len >= sizeof(_b))
      len = sizeof(_b) - _l - 1;

    memcpy(_b + _l, str, len);
    _l += len;
    _b[_l] = 0;
  }

  void overlay(char const *str, unsigned len)
  {
    if (len + 1 > sizeof(_b))
      len = sizeof(_b) - 1;

    if (len < _l)
      return;

    str += _l;
    len -= _l;

    memcpy(_b + _l, str, len);
    _l = len + _l;
  }

};


PRIVATE static
bool
Jdb::input_long_mode(Jdb::Cmd *cmd, char const **args)
{
  static Cmd_buffer buf;
  buf.flush();
  for (;;)
    {
      int c = getchar();

      switch (c)
	{
	case KEY_BACKSPACE:
	case KEY_BACKSPACE_2:
	  if (buf.len() > 0)
	    {
	      cursor(Cursor_left);
	      clear_to_eol();
	      buf.cut(-1);
	    }
	  continue;

	case KEY_TAB:
	    {
	      bool multi_match = false;
	      *cmd = Jdb_core::complete_cmd(buf.buffer(), multi_match);
	      if (cmd->cmd && multi_match)
		{
		  printf("\n");
		  unsigned prefix_len = Jdb_core::print_alternatives(buf.buffer());
		  print_prompt();
		  buf.overlay(cmd->cmd->cmd, prefix_len);
		  putnstr(buf.buffer(), buf.len());
		}
	      else if (cmd->cmd)
		{
		  putstr(cmd->cmd->cmd + buf.len());
		  putchar(' ');
		  buf.overlay(cmd->cmd->cmd, strlen(cmd->cmd->cmd));
		  buf.append(' ');
		}
	      continue;
	    }

	case KEY_RETURN:
	case KEY_RETURN_2:
	  puts("");
	  if (!buf.len())
	    {
	      hide_statline = false;
	      cmd->cmd = nullptr;
	      return false;
	    }
	  break;

	default:
          if (c >= 0x100) // see keycodes.h: no special keys on command line
            continue;

	  buf.append(c);
	  printf("\033[K%c", c);
	  continue;
	}

      *cmd = Jdb_core::has_cmd(buf.buffer());
      if (cmd->cmd)
	{
	  unsigned cmd_len = strlen(cmd->cmd->cmd);
	  *args = buf.buffer() + cmd_len;
	  while (**args == ' ')
	    ++(*args);
	  return false; // do not leave the debugger
	}
      else
	{
	  printf("unknown command: '%s'\n", buf.buffer());
	  print_prompt();
	  buf.flush();
	}
    }
}

PRIVATE static
int
Jdb::execute_command()
{
  char const *args;
  Jdb_core::Cmd cmd(nullptr,nullptr);
  bool leave;
  int cmd_key = 0;

  if (short_mode)
    leave = input_short_mode(&cmd, &args, cmd_key);
  else
    leave = input_long_mode(&cmd, &args);

  if (leave)
    return 0;

  if (cmd.cmd)
    {
      int ret = Jdb_core::exec_cmd(cmd, args);

      if (!ret)
	hide_statline = false;

      return ret;
    }

  return 1;
}

PRIVATE static
bool
Jdb::open_debug_console(Cpu_number cpu)
{
  in_service = 1;
  __libc_backend_printf_local_force_unlock();
  save_disable_irqs(cpu);
  if (cpu == Cpu_number::boot_cpu())
    jdb_enter.execute();

  if (!stop_all_cpus(cpu))
    return false; // CPUs other than 0 never become interactive

  if (!Jdb_screen::direct_enabled())
    Kconsole::console()->
      change_state(Console::DIRECT, 0, ~Console::OUTENABLED, 0);

  return true;
}


PRIVATE static
void
Jdb::close_debug_console(Cpu_number cpu)
{
  Proc::cli();
  Mem::barrier();
  if (cpu == Cpu_number::boot_cpu())
    {
      in_jdb.cpu(cpu) = false;
      // eat up input from console
      while (Kconsole::console()->getchar(false)!=-1)
	;

      Kconsole::console()->
	change_state(Console::DIRECT, 0, ~0UL, Console::OUTENABLED);

      in_service = 0;
      leave_wait_for_others();
      jdb_leave.execute();
    }

  Mem::barrier();
  restore_irqs(cpu);
}

PUBLIC static
void
Jdb::remote_work(Cpu_number cpu, cxx::functor<void (Cpu_number)> &&func,
                 bool sync = true)
{
  if (!Cpu::online(cpu))
    return;

  if (cpu == Cpu_number::boot_cpu())
    func(cpu);
  else
    {
      Jdb::Remote_func &rf = Jdb::remote_func.cpu(cpu);
      rf.wait();
      rf.set_mp_safe(func);

      if (sync)
        rf.wait();
    }
}

PUBLIC template<typename Func> static
void
Jdb::on_each_cpu(Func &&func, bool single_sync = true)
{
  foreach_cpu([&](Cpu_number cpu){ remote_work(cpu, cxx::forward<Func>(func), single_sync); });
}

PUBLIC template<typename Func> static
void
Jdb::on_each_cpu_pl(Func &&func)
{
  foreach_cpu([&](Cpu_number cpu){ remote_work(cpu, cxx::forward<Func>(func), false); });

  foreach_cpu([](Cpu_number cpu){ Jdb::remote_func.cpu(cpu).wait(); });
}

PUBLIC
static int
Jdb::getchar(void)
{
  int c = Jdb_core::getchar();
  force_app_cpus_into_jdb(false);
  return c;
}

IMPLEMENT
void Jdb::cursor_home()
{
  putstr("\033[H");
}

IMPLEMENT
void Jdb::cursor_end_of_screen()
{
  putstr("\033[127;1H");
}

//-------- pretty print functions ------------------------------
PUBLIC static
void
Jdb::write_ll_ns(String_buffer *buf, Signed64 ns, bool sign)
{
  Unsigned64 uns = (ns < 0) ? -ns : ns;

  if (uns >= 3'600'000'000'000'000ULL)
    {
      buf->printf(">999 h ");
      return;
    }

  if (sign)
    buf->printf("%c", (ns < 0) ? '-' : (ns == 0) ? ' ' : '+');

  if (uns >= 60'000'000'000'000ULL)
    {
      // 1000min...999h
      Mword _h  = uns / 3'600'000'000'000ULL;
      Mword _m  = (uns % 3'600'000'000'000ULL) / 60'000'000'000ULL;
      buf->printf("%3lu:%02lu h  ", _h, _m);
      return;
    }

  if (uns >= 1'000'000'000'000ULL)
    {
      // 1000s...999min
      Mword _m  = uns / 60'000'000'000ULL;
      Mword _s  = (uns % 60'000'000'000ULL) / 1'000ULL;
      buf->printf("%3lu:%02lu M  ", _m, _s);
      return;
    }

  if (uns >= 1'000'000'000ULL)
    {
      // 1...1000s
      Mword _s  = uns / 1'000'000'000ULL;
      Mword _ms = (uns % 1'000'000'000ULL) / 1'000'000ULL;
      buf->printf("%3lu.%03lu s ", _s, _ms);
      return;
    }

  if (uns >= 1'000'000)
    {
      // 1...1000ms
      Mword _ms = uns / 1'000'000UL;
      Mword _us = (uns % 1'000'000UL) / 1'000UL;
      buf->printf("%3lu.%03lu ms", _ms, _us);
      return;
    }

  if (uns == 0)
    {
      buf->printf("  0       ");
      return;
    }

  Mword _us = uns / 1'000UL;
  Mword _ns = uns % 1'000UL;
  buf->printf("%3lu.%03lu u ", _us, _ns);
}

PUBLIC static
void
Jdb::write_us_shortfmt(String_buffer *buf, Unsigned32 us)
{
  if (us >= 100'000'000)
    buf->printf(">99s");
  else if (us >= 10'000'000)
    buf->printf("%3us", us / 1'000'000);
  else if (us >= 1'000'000)
    buf->printf("%u.%us", us / 1'000'000, (us % 1'000'000) / 100'000);
  else if (us >= 10'000)
    buf->printf("%um", us / 1000);
  else if (us >= 1'000)
    buf->printf("%u.%um", us / 1000, (us % 1000) / 100);
  else
    buf->printf("%3uu", us);
}

PUBLIC static
void
Jdb::write_ll_hex(String_buffer *buf, Signed64 x, bool sign)
{
  // display 40 bits
  Unsigned64 xu = (x < 0) ? -x : x;

  if (sign)
    buf->printf("%s%03llx%08llx",
                (x < 0) ? "-" : (x == 0) ? " " : "+",
                (xu >> 32) & 0xfffU, xu & 0xffffffffU);
  else
    buf->printf("%04llx%08llx", (xu >> 32) & 0xffffU, xu & 0xffffffffU);
}

PUBLIC static
void
Jdb::write_ll_dec(String_buffer *buf, Signed64 x, bool sign)
{
  Unsigned64 xu = (x < 0) ? -x : x;

  // display no more than 11 digits
  if (xu >= 100000000000ULL)
    {
      buf->printf("%12s", ">= 10^11");
      return;
    }

  if (sign && x != 0)
    buf->printf("%+12lld", x);
  else
    buf->printf("%12llu", xu);
}

PUBLIC static
void
Jdb::cpu_mask_print(Cpu_mask &m)
{
  Cpu_number start = Cpu_number::nil();
  bool first = true;
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    {
      if (m.get(i) && start == Cpu_number::nil())
        start = i;

      bool last = i + Cpu_number(1) == Config::max_num_cpus();
      if (start != Cpu_number::nil() && (!m.get(i) || last))
        {
          printf("%s%u", first ? "" : ",", cxx::int_value<Cpu_number>(start));
          first = false;
          if (i - Cpu_number(!last) > start)
            printf("-%u", cxx::int_value<Cpu_number>(i) - !(last && m.get(i)));

          start = Cpu_number::nil();
        }
    }
}

IMPLEMENT_DEFAULT
void
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 uns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);

  if (tsc < 0)
    uns = -uns;

  if (sign)
    buf->printf("%c", (tsc < 0) ? '-' : (tsc == 0) ? ' ' : '+');

  Mword _s  = uns / 1000000000;
  Mword _us = (uns / 1000) - 1000000 * _s;
  buf->printf("%3lu.%06lu s ", _s, _us);
  return;
}

IMPLEMENT_DEFAULT
void
Jdb::write_tsc(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 ns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);
  if (tsc < 0)
    ns = -ns;
  write_ll_ns(buf, ns, sign);
}

PUBLIC static inline
Jdb_entry_frame*
Jdb::get_entry_frame(Cpu_number cpu)
{
  return entry_frame.cpu(cpu);
}

/// handling of standard cursor keys (Up/Down/PgUp/PgDn)
PUBLIC static
int
Jdb::std_cursor_key(int c, Mword cols, Mword lines,
                    Mword max_absy, Mword max_pos,
                    Mword *absy, Mword *addy, Mword *addx, bool *redraw)
{
  Mword old_absy = *absy;
  Mword old_pos  = (*absy + *addy) * cols + (addx ? *addx : 0);
  if (!max_pos)
    max_pos = (max_absy + lines-1) * cols-1;
  switch (c)
    {
    case KEY_CURSOR_LEFT:
    case 'h':
      if (!addx)
        return 0;
      if (*addx > 0)
        (*addx)--;
      else if (*addy > 0)
        (*addy)--, *addx = cols-1;
      else if (*absy > 0)
        (*absy)--, *addx = cols-1;
      break;
    case KEY_CURSOR_RIGHT:
    case 'l':
      if (!addx)
        return 0;
      if (*addx < cols-1 && old_pos+1 <= max_pos)
        (*addx)++;
      else if (*addy < lines-1)
        (*addy)++, *addx = 0;
      else if (*absy < max_absy)
        (*absy)++, *addx = 0;
      break;
    case KEY_CURSOR_UP:
    case 'k':
      if (*addy > 0)
        (*addy)--;
      else if (*absy > 0)
        (*absy)--;
      break;
    case KEY_CURSOR_DOWN:
    case 'j':
      if (*addy < lines-1 && old_pos + cols <= max_pos)
        (*addy)++;
      else if (*absy < max_absy && old_pos + cols <= max_pos)
        (*absy)++;
      else if (*absy < max_absy)
        (*absy)++, (*addy)--;
      break;
    case KEY_CURSOR_HOME:
    case 'H':
      if (addx)
        *addx = 0;
      *absy = 0;
      *addy = 0;
      break;
    case KEY_CURSOR_END:
    case 'L':
      if (addx)
        *addx = max_pos % cols;
      *absy = max_absy;
      *addy = lines-1;
      break;
    case KEY_PAGE_UP:
    case 'K':
      if (*absy >= lines)
        *absy -= lines;
      else if (*absy > 0)
        *absy = 0;
      else if (*addy > 0)
        *addy = 0;
      else if (addx)
        *addx = 0;
      break;
    case KEY_PAGE_DOWN:
    case 'J':
      if (*absy+lines-1 < max_absy && old_pos + lines * cols <= max_pos)
        *absy += lines;
      else if (*absy < max_absy)
        *absy = max_absy;
      else if (*addy < lines-1 && old_pos + (lines-1 - *addy) * cols <= max_pos)
        *addy = lines-1;
      else if (*addy < lines - 2)
        *addy = lines-2;
      else if (addx && old_pos + cols - 1 <= max_pos)
        *addx = cols - 1;
      else if (addx && *addy == lines - 1)
        *addx = max_pos % cols;
      break;
    default:
      return 0;
    }

  *redraw = *absy != old_absy;
  return 1;
}

PUBLIC static inline
Space *
Jdb::get_task(Cpu_number cpu)
{
  if (!get_thread(cpu))
    return nullptr;
  else
    return get_thread(cpu)->space();
}


//
// memory access wrappers
//

/** Read or write memory without crossing a page boundary. */
PRIVATE static
template < typename T >
int
Jdb::peek_or_poke_task(Jdb_address addr, T *value, size_t bytes)
{
  bool do_write = cxx::is_same_v<T, void const>;
  unsigned char *mem = access_mem_task(addr, do_write);
  if (!mem)
    return -1;
  size_t bytes_to_copy = bytes;

  if (Pg::trunc(addr.addr()) != Pg::trunc(addr.addr() + bytes))
    bytes_to_copy = Pg::round(addr.addr()) - addr.addr();
  if constexpr (cxx::is_same_v<T, void>)
    memcpy(value, mem, bytes_to_copy);
  else
    {
      memcpy(mem, value, bytes_to_copy);
      Mem_unit::make_coherent_to_pou(mem, bytes_to_copy);
    }

  if (bytes_to_copy != bytes)
    {
      mem = access_mem_task(addr, do_write);
      if (!mem)
        return -1;
      addr += bytes_to_copy;
      bytes_to_copy = bytes - bytes_to_copy;
      if constexpr (cxx::is_same_v<T, void>)
        memcpy(value, mem, bytes_to_copy);
      else
        {
          memcpy(mem, value, bytes_to_copy);
          Mem_unit::make_coherent_to_pou(mem, bytes_to_copy);
        }
    }
  return 0;
}

PUBLIC static
int
Jdb::peek_task(Jdb_address addr, void *value, size_t width)
{ return peek_or_poke_task(addr, value, width); }

PUBLIC static
int
Jdb::poke_task(Jdb_address addr, void const *value, size_t width)
{ return peek_or_poke_task(addr, value, width); }

PUBLIC static
template< typename T >
bool
Jdb::peek(Jdb_addr<T> addr, cxx::remove_const_t<T> &value)
{
  T tmp;
  bool ret = peek_task(addr, &tmp, sizeof(T)) == 0;
  value = tmp;
  return ret;
}

PUBLIC static
template< typename T >
bool
Jdb::poke(Jdb_addr<T> addr, T const &value)
{ return poke_task(addr, &value, sizeof(T)) == 0; }


class Jdb_base_cmds : public Jdb_module
{
public:
  Jdb_base_cmds() FIASCO_INIT;
};

static Jdb_base_cmds jdb_base_cmds INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PUBLIC
Jdb_module::Action_code
Jdb_base_cmds::action (int cmd, void *&, char const *&, int &) override
{
  if (cmd!=0)
    return NOTHING;

  Jdb_core::short_mode = !Jdb_core::short_mode;
  printf("\ntoggle mode: now in %s command mode (use '%s' to switch back)\n",
         Jdb_core::short_mode ? "short" : "long",
         Jdb_core::short_mode ? "*" : "mode");
  return NOTHING;
}

PUBLIC
int
Jdb_base_cmds::num_cmds() const override
{ 
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_base_cmds::cmds() const override
{
  static Cmd cs[] =
    { { 0, "*", "mode", "", "*|mode\tswitch long and short command mode",
        nullptr } };

  return cs;
}

IMPLEMENT
Jdb_base_cmds::Jdb_base_cmds()
  : Jdb_module("GENERAL")
{}

PRIVATE inline static
void
Jdb::rcv_uart_enable()
{
  if (Config::serial_esc == Config::SERIAL_ESC_IRQ)
    Kernel_uart::enable_rcv_irq();
}

char Jdb::esc_iret[]     = "\033[36;1m";
char Jdb::esc_bt[]       = "\033[31m";
char Jdb::esc_emph[]     = "\033[33;1m";
char Jdb::esc_emph2[]    = "\033[32;1m";
char Jdb::esc_mark[]     = "\033[35;1m";
char Jdb::esc_line[]     = "\033[37m";
char Jdb::esc_symbol[]   = "\033[33;1m";


IMPLEMENT int
Jdb::enter_jdb(Trap_state *ts, Cpu_number cpu)
{
  static_assert(sizeof(Jdb_entry_frame) == sizeof(Trap_state));
  auto *e = static_cast<Jdb_entry_frame *>(ts);

  if (e->debug_ipi())
    {
      if (!remote_work_ipi_process(cpu))
        return 0;
      if (!in_service)
	return 0;
    }

  if (handle_early_debug_traps(e))
    return 0;

  enter_trap_handler(cpu);

  if (handle_conditional_breakpoint(cpu, e))
    {
      // don't enter debugger, only logged breakpoint
      leave_trap_handler(cpu);
      return 0;
    }

  if (!in_jdb.cpu(cpu))
    entry_frame.cpu(cpu) = e;

  volatile bool really_break = true;

  static jmp_buf recover_buf;
  static Jdb_entry_frame nested_trap_frame;

  if (in_jdb.cpu(cpu))
    {
      nested_trap_frame = *e;

      // Since we entered the kernel debugger a second time,
      // Thread::nested_trap_recover
      // has a value of 2 now. We don't leave this function so correct the
      // entry counter
      Thread::nested_trap_recover.cpu(cpu)--;

      longjmp(recover_buf, 1);
    }

  // all following exceptions are handled by jdb itself
  in_jdb.cpu(cpu) = true;

  // If entered by Ipi::Debug, a thread on another CPU requested this CPU to
  // enter. Otherwise, we entered by exception. Remember the first one.
  if (!e->debug_ipi() && triggered_on_cpu == Cpu_number::nil())
    triggered_on_cpu = cpu;

  if (!open_debug_console(cpu))
    { // not on the master CPU just wait
      close_debug_console(cpu);
      leave_trap_handler(cpu);
      return 0;
    }

  // As of here, we are certain that this is the boot CPU!

  store_system_clock_on_enter();

  if (triggered_on_cpu == Cpu_number::nil())
    triggered_on_cpu = Cpu_number::boot_cpu(); // should not happen

  // check for kdb_ke debugging interface; only used from kernel context
  if (foreach_cpu(&handle_user_request, true))
    {
      close_debug_console(cpu);
      leave_trap_handler(cpu);
      triggered_on_cpu = Cpu_number::nil();
      clear_system_clock_on_enter();
      return 0;
    }

  hide_statline = false;

  error_buffer.cpu(cpu).clear();

  really_break = foreach_cpu(&handle_debug_traps, false);

  while (setjmp(recover_buf))
    {
      // handle traps which occurred while we are in Jdb
      Kconsole::console()->end_exclusive(Console::GZIP);
      handle_nested_trap(&nested_trap_frame);
    }

  if (!never_break && really_break) 
    {
      do
	{
	  screen_scroll(1, Jdb_screen::height());
	  if (!hide_statline)
	    {
	      cursor(Jdb_screen::height(), 1);
	      printf("\n%s%s    %.*s\033[m      \n",
	             esc_prompt,
	             test_checksums()
	               ? ""
	               : "    WARNING: Fiasco kernel checksum differs -- "
	                 "read-only data has changed!\n",
	             Jdb_screen::width() - 11,
	             Jdb_screen::Line);

              Cpu_mask cpus_in_jdb;
              int cpu_cnt = 0;
	      for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
		if (Cpu::online(i))
		  {
		    if (in_jdb.cpu(i))
                      {
                        ++cpu_cnt;
                        cpus_in_jdb.set(i);
                        if (!entry_frame.cpu(i)->debug_ipi())
                          printf("    CPU%2u [" L4_PTR_FMT "]: %s\n",
                                 cxx::int_value<Cpu_number>(i),
                                 entry_frame.cpu(i)->ip(),
                                 error_buffer.cpu(i).c_str());
                      }
		    else
		      printf("    CPU%2u: is not in JDB (not responding)\n",
                             cxx::int_value<Cpu_number>(i));
		  }
              if (!cpus_in_jdb.empty() && cpu_cnt > 1)
                {
                  printf("    CPU(s) ");
                  cpu_mask_print(cpus_in_jdb);
                  printf(" entered JDB\n");
                }
	      hide_statline = true;
	    }

	  printf_statline(nullptr, nullptr, "_");

	} while (execute_command());

      // reset scrolling region of serial terminal
      screen_scroll(1,127);

      // reset cursor
      blink_cursor(Jdb_screen::height(), 1);

      // goto end of screen
      Jdb::cursor(127, 1);
    }

  // re-enable interrupts
  triggered_on_cpu = Cpu_number::nil();
  close_debug_console(cpu);

  rcv_uart_enable();

  clear_system_clock_on_enter();
  leave_trap_handler(cpu);
  return 0;
}

PUBLIC static
const char *
Jdb::addr_space_to_str(Jdb_address addr, char *str, size_t len)
{
  if (addr.is_kmem())
    return "kernel";
  if (addr.is_phys())
    return "physical";
  snprintf(str, len, "task D:%lx",
           static_cast<Task*>(addr.space())->dbg_info()->dbg_id());
  return str;
}


//--------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PRIVATE static
bool
Jdb::stop_all_cpus(Cpu_number)
{ return true; }

PRIVATE
static
void
Jdb::leave_wait_for_others()
{}

PRIVATE static
void
Jdb::force_app_cpus_into_jdb(bool)
{}

PRIVATE static inline
int
Jdb::remote_work_ipi_process(Cpu_number)
{ return 1; }


//---------------------------------------------------------------------------
INTERFACE [mp]:

#include "spin_lock.h"

EXTENSION class Jdb
{
  // remote call
  static Spin_lock<> _remote_call_lock;
  static void (*_remote_work_ipi_func)(Cpu_number, void *);
  static void *_remote_work_ipi_func_data;
  static unsigned long _remote_work_ipi_done;
  // non-boot CPUs halting in JDB
  static void other_cpu_halt_in_jdb();
  // wakeup non-boot CPUs from halting
  static void wakeup_other_cpus_from_jdb(Cpu_number);
};

//--------------------------------------------------------------------------
IMPLEMENTATION [mp]:

void (*Jdb::_remote_work_ipi_func)(Cpu_number, void *);
void *Jdb::_remote_work_ipi_func_data;
unsigned long Jdb::_remote_work_ipi_done;
Spin_lock<> Jdb::_remote_call_lock;

/**
 * Waits for all the given application CPUs to enter the debugger with a timeout
 * of 1s. Also sets `cpus_in_debugger` which is used while leaving the debugger.
 *
 * \param online_cpus_to_stop  The CPUs to wait for to enter the debugger.
 *
 * \retval 0  All CPUs entered the debugger.
 * \retval 1  Another CPU just finished booting, wait aborted.
 * \retval 2  Timeout hit, some CPU(s) did not respond.
 */
PRIVATE static
int
Jdb::wait_for_app_cpus(Cpu_mask const &online_cpus_to_stop)
{
  enum { Max_wait_cnt = 1000 };

  unsigned long wait_cnt = 0;
  for (;;)
    {
      bool all_there = true;
      cpus_in_debugger = 0;

      Mem::barrier(); // other CPUs might have changed state

      for (Cpu_number c = Cpu_number::second(); c < Config::max_num_cpus(); ++c)
        if (Cpu::online(c))
          {
            if (!online_cpus_to_stop.get(c))
              return 1; // another CPU just finished booting
            if (!in_jdb.cpu(c))
              all_there = false;
            else
              ++cpus_in_debugger;
          }

      if (all_there)
        return 0;

      Proc::pause();
      if (++wait_cnt == Max_wait_cnt)
        return 2;

      Delay::delay(1);
    }
}

/**
 * Force all applications CPUs into the debugger by sending a debug IPI and, if
 * that doesn't help, by sending an NMI.
 *
 * \param try_nmi  Try harder by sending an NMI.
 */
PRIVATE static
void
Jdb::force_app_cpus_into_jdb(bool try_nmi)
{
  Cpu_mask online_cpus_to_stop;
  Cpu_mask cpus_nmi_notified;

  bool timed_out = false;
  for (;;)
    {
      bool did_notify = false;
      for (Cpu_number c = Cpu_number::second(); c < Config::max_num_cpus(); ++c)
        {
          if (!Cpu::online(c) || in_jdb.cpu(c))
            continue;

          if (!online_cpus_to_stop.get(c))
            {
              // Initially try to notify CPU via debug IPI.
              Ipi::send(Ipi::Debug, Cpu_number::first(), c);
              online_cpus_to_stop.set(c);
              did_notify = true;
            }
          else if (timed_out && !cpus_nmi_notified.get(c))
            {
              // Timeout: Report that and try to notify CPU via NMI.
              printf("JDB: CPU%u: is not responding ... %s\n",
                     cxx::int_value<Cpu_number>(c), try_nmi ? "trying NMI" : "");
              if (try_nmi)
                {
                  send_nmi(c);
                  did_notify = true;
                }
              cpus_nmi_notified.set(c);
            }
        }

      if (!did_notify)
        return; // either timed out or there are no other online CPUs

      switch (wait_for_app_cpus(online_cpus_to_stop))
        {
        case 0: // all CPUs in debugger
          return; // done

        case 1: // another CPU just finished booting
          timed_out = false;
          break; // retry

        case 2: // some CPUs did not respond in time
          timed_out = true;
          break; // retry with NMI if requested via try_nmi
        }
    }
}

PRIVATE static
bool
Jdb::stop_all_cpus(Cpu_number current_cpu)
{
  enum { Max_wait_cnt = 1000 };
  // JDB always runs on the boot CPU, if any other CPU enters the debugger
  // the boot CPU is notified to do enter the debugger too
  if (current_cpu == Cpu_number::boot_cpu())
    {
      // I'm CPU 0 stop all other CPUs and wait for them to enter the JDB
      jdb_active = 1;
      Mem::barrier();
      force_app_cpus_into_jdb(true);
      // All CPUs entered JDB, so go on and become interactive
      return true;
    }
  else
    {
      // Huh, not CPU 0, so notify CPU 0 to enter JDB too
      // The notification is ignored if CPU 0 is already within JDB
      jdb_active = true;
      Ipi::send(Ipi::Debug, current_cpu, Cpu_number::boot_cpu());

      unsigned long wait_count = Max_wait_cnt;
      while (!in_jdb.cpu(Cpu_number::boot_cpu()) && wait_count)
        {
          Proc::pause();
          Delay::delay(1);
          Mem::barrier();
          --wait_count;
        }

      if (wait_count == 0)
        send_nmi(Cpu_number::boot_cpu());

      // Wait for messages from CPU 0
      while (access_once(&jdb_active))
        {
          Mem::mp_mb();
          remote_func.cpu(current_cpu).monitor_exec(current_cpu);
          other_cpu_halt_in_jdb();
        }

      // This CPU defacto left JDB
      in_jdb.cpu(current_cpu) = false;

      // Signal CPU 0, that we are ready to leve the debugger
      // This is the second door of the airlock
      atomic_add(&cpus_in_debugger, -1UL);

      // Wait for CPU 0 to leave us out
      while (access_once(&leave_barrier))
        {
          Mem::barrier();
          Proc::pause();
        }

      // CPU 0 signaled us to leave JDB
      return false;
    }
}

PRIVATE
static
void
Jdb::leave_wait_for_others()
{
  leave_barrier = 1;
  jdb_active = 0;
  Mem::barrier();
  for (;;)
    {
      bool all_there = true;
      for (Cpu_number c = Cpu_number::first(); c < Config::max_num_cpus(); ++c)
	{
	  if (cpu_in_jdb(c))
	    {
	      // notify other CPU
              wakeup_other_cpus_from_jdb(c);
              Jdb::remote_func.cpu(c).reset_mp_safe();
//	      printf("JDB: wait for CPU[%2u] to leave\n", cxx::int_value<Cpu_number>(c));
	      all_there = false;
	    }
	}

      if (!all_there)
	{
	  Proc::pause();
	  Mem::mp_mb();
	  continue;
	}

      break;
    }

  while (access_once(&cpus_in_debugger))
    {
      Mem::mp_mb();
      Proc::pause();
    }

  Mem::barrier();
  leave_barrier = 0;
}

// The remote_work_ipi* functions are for the IPI round-trip benchmark (only)
PRIVATE static
int
Jdb::remote_work_ipi_process(Cpu_number cpu)
{
  if (_remote_work_ipi_func)
    {
      _remote_work_ipi_func(cpu, _remote_work_ipi_func_data);
      Mem::barrier();
      _remote_work_ipi_done = 1;
      return 0;
    }
  return 1;
}

PUBLIC static
bool
Jdb::remote_work_ipi(Cpu_number this_cpu, Cpu_number to_cpu,
                     void (*f)(Cpu_number, void *), void *data, bool wait = true)
{
  if (to_cpu == this_cpu)
    {
      f(this_cpu, data);
      return true;
    }

  if (!Cpu::online(to_cpu))
    return false;

  auto guard = lock_guard(_remote_call_lock);

  _remote_work_ipi_func      = f;
  _remote_work_ipi_func_data = data;
  _remote_work_ipi_done      = 0;

  Ipi::send(Ipi::Debug, this_cpu, to_cpu);

  if (wait)
    while (!access_once(&_remote_work_ipi_done))
      Proc::pause();

  _remote_work_ipi_func = nullptr;

  return true;
}

IMPLEMENT_DEFAULT
void
Jdb::other_cpu_halt_in_jdb()
{
  Proc::pause();
}

IMPLEMENT_DEFAULT
void
Jdb::wakeup_other_cpus_from_jdb(Cpu_number)
{}
