IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include <cxx/conditionals>

using cxx::const_ite;

#include "config.h"
#include "cpu.h"
#include "jdb.h"
#include "jdb_disasm.h"
#include "jdb_input.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_regex.h"
#include "jdb_tbuf.h"
#include "jdb_tbuf_output.h"
#include "kern_cnt.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "perf_cnt.h"
#include "simpleio.h"
#include "static_init.h"
#include "thread.h"

class Entry_group
{
public:
  enum { Max_group_size = 10 };
  typedef Tb_entry::Group_order Group_order;

  struct Item
  {
    Tb_entry const *e;
    size_t y;
    Group_order order;

    Item() : e(nullptr), y(0), order(Group_order::none()) {}
  };

  Entry_group() : _c(0) {}

  Item const &operator [] (unsigned i) const & { return _i[i]; }
  Item const &operator [] (unsigned i) const && = delete;
  Item &operator [] (unsigned i) & { return _i[i]; }
  unsigned size() const { return _c; }
  bool full() const { return _c >= Max_group_size; }

  unsigned push_back(Tb_entry const *e, size_t y, Group_order t)
  {
    unsigned p = _c++;
    Item *u = &_i[p];
    u->e = e;
    u->y = y;
    u->order = t;
    return p;
  }

private:
  unsigned _c;
  Item _i[Max_group_size];
};

class Jdb_tbuf_show : public Jdb_module
{
public:
  Jdb_tbuf_show() FIASCO_INIT;

private:
  static char  _search_str[40];
  static char  _filter_str[40];
  static String_buf<512> _buffer_str;
  static unsigned _status_type;
  static Mword _absy;
  static Tb_sequence _seq_cur;
  static Tb_sequence _seq_ref;
  static Tb_sequence _seq_pos[10];
  static size_t y_offset;

  enum
  {
    Index_mode        = 0, // number of event
    Tsc_delta_mode    = 1, // tsc ticks starting from last event
    Tsc_ref_mode      = 2, // tsc ticks starting from reference event
    Tsc_start_mode    = 3, // tsc ticks starting from 0
    Kclock_ref_mode   = 4, // kernel clock units (us) since reference event
    Kclock_start_mode = 5, // kernel clock units (us) since start of system
    Pmc1_delta_mode   = 6, // ticks of ctr 1 starting from last event
    Pmc2_delta_mode   = 7, // ticks of ctr 2 starting from last event
    Pmc1_ref_mode     = 8, // ticks of ctr 1 starting from reference event
    Pmc2_ref_mode     = 9, // ticks of ctr 2 starting from reference event
  };

  enum
  {
    Tbuf_start_line   = 3,
  };

  enum : unsigned
  {
    Status_ok         = 0,
    Status_redraw     = 1,
    Status_error      = 2,
  };

  enum : Mword
  {
    No_mask = ~Mword{0U}
  };
};

char Jdb_tbuf_show::_search_str[40];
char Jdb_tbuf_show::_filter_str[40];
String_buf<512> Jdb_tbuf_show::_buffer_str;
unsigned Jdb_tbuf_show::_status_type;
Mword Jdb_tbuf_show::_absy;
Tb_sequence Jdb_tbuf_show::_seq_cur;
Tb_sequence Jdb_tbuf_show::_seq_ref;

Tb_sequence Jdb_tbuf_show::_seq_pos[10] =
{
  Jdb_tbuf::Nil, Jdb_tbuf::Nil, Jdb_tbuf::Nil, Jdb_tbuf::Nil, Jdb_tbuf::Nil,
  Jdb_tbuf::Nil, Jdb_tbuf::Nil, Jdb_tbuf::Nil, Jdb_tbuf::Nil, Jdb_tbuf::Nil
};

size_t Jdb_tbuf_show::y_offset = 0;

static void
Jdb_tbuf_show::error(const char * const msg)
{
  Jdb::printf_statline("tbuf", nullptr, "\033[31;1m=== %s! ===\033[m", msg);
  _status_type = Status_error;
}

static void
Jdb_tbuf_show::show_perf_event_unit_mask_entry(Mword nr, Mword idx,
                                               Mword unit_mask, bool exclusive)
{
  const char *desc = nullptr;
  Mword value = 0;

  Perf_cnt::get_unit_mask_entry(nr, idx, &value, &desc);
  if (!desc || !*desc)
    desc = "(no description?)";
  printf("  %c %02lx %.59s\033[K",
         exclusive ? unit_mask == value ? '+' : ' '
                   : unit_mask  & value ? '+' : ' ',
         value, desc);
}

static void
Jdb_tbuf_show::show_perf_event(Mword nr)
{
  const char *name = nullptr, *desc = nullptr;
  unsigned evntsel = 0;
  Mword add_kcnt = const_ite<Config::Jdb_accounting>(Kern_cnt::Valid_ctrs, 0);

  if (nr < add_kcnt)
    {
      const char * const s = Kern_cnt::get_vld_str(nr);
      printf("   %-26.26s %.49s\033[K", s, "(kernel event counter)");
      return;
    }

  Perf_cnt::get_perf_event(nr - add_kcnt, &evntsel, &name, &desc);
  if (!name || !*name)
    name = "(no name?)";
  if (!desc || !*desc)
    desc = "(no description)";

  printf("%02x %-26.26s %.49s\033[K", evntsel, name, desc);
}

static Mword
Jdb_tbuf_show::select_perf_event_unit_mask(Mword nr, Mword unit_mask)
{
  Mword absy = 0;
  Mword addy = 0;
  Mword max_absy = 0;
  unsigned lines = 10;

  Mword default_value = 0, nvalues = 0, value = 0;
  Perf_cnt::Unit_mask_type type = Perf_cnt::None;
  const char *desc = nullptr;

  Perf_cnt::get_unit_mask(nr, &type, &default_value, &nvalues);
  bool exclusive = (type == Perf_cnt::Exclusive);

  if (type == Perf_cnt::None)
    return 0;

  if (type == Perf_cnt::Fixed || (nvalues < 1))
    return default_value;

  if (nvalues == 1)
    {
      Perf_cnt::get_unit_mask_entry(nr, 0, &value, &desc);
      return value;
    }

  if (nvalues < lines)
    lines = nvalues;

  Jdb::printf_statline("tbuf", "<Space>=set mask <CR>=done", "P?");

  Jdb::cursor(Tbuf_start_line, 1);
  putstr("\033[32m");
  show_perf_event(nr
                  + const_ite<Config::Jdb_accounting>(Kern_cnt::Valid_ctrs, 0));
  printf("\033[m\033[K\n"
         "\033[K\n"
         "  \033[1;32mSelect Event Mask (%s):\033[m\033[K\n"
         "\033[K\n", exclusive ? "exclusive" : "bitmap");

  for (;;)
    {
      unsigned i;

      Jdb::cursor(Tbuf_start_line + 4, 1);
      for (i = 0; i < lines; ++i)
        {
          show_perf_event_unit_mask_entry(nr, i, unit_mask, exclusive);
          putchar('\n');
        }

      for (; i < Jdb_screen::height() - Tbuf_start_line - 5; ++i)
        puts("\033[K");

      for (bool redraw = false; !redraw;)
        {
          int c;
          const char *dummy;
          Mword value;

          Jdb::cursor(addy + Tbuf_start_line + 4, 1);
          putstr(Jdb::esc_emph);
          show_perf_event_unit_mask_entry(nr, absy + addy, unit_mask, exclusive);
          putstr("\033[m");
          Jdb::cursor(addy + Tbuf_start_line + 4, 1);
          c = Jdb_core::getchar();
          show_perf_event_unit_mask_entry(nr, absy + addy, unit_mask, exclusive);
          Perf_cnt::get_unit_mask_entry(nr, absy + addy, &value, &dummy);

          if (Jdb::std_cursor_key(c, 0, lines, max_absy, 0,
                                  &absy, &addy, nullptr, &redraw))
            continue;

          switch (c)
           {
            case ' ':
              if (exclusive)
                {
                  unit_mask = value;
                  redraw = true;
                }
              else
                unit_mask ^= value;
              break;
            case KEY_RETURN:
            case KEY_RETURN_2:
              return unit_mask;
            case KEY_ESC:
              return No_mask;
            default:
              if (Jdb::is_toplevel_cmd(c))
                return No_mask;
            }
        }
    }
}

static Mword
Jdb_tbuf_show::select_perf_event(Mword event)
{
  Mword absy     = 0;
  Mword addy     = 0;
  Mword add_kcnt = const_ite<Config::Jdb_accounting>(Kern_cnt::Valid_ctrs, 0);
  Mword nevents  = Perf_cnt::get_max_perf_event() + add_kcnt;
  Mword lines    = (nevents < Jdb_screen::height() - 6)
                   ? nevents
                   : Jdb_screen::height() - 6;
  Mword max_absy = nevents-lines;

  if (nevents == 0)
    // libperfctr not linked
    return Perf_cnt::No_event;

  unsigned evntsel;
  Mword unit_mask;

  Jdb::printf_statline("tbuf", "<CR>=select", "P?");

  Jdb::cursor(Tbuf_start_line, 1);
  printf("%sSelect Performance Counter\033[m\033[K\n\033[K", Jdb::esc_emph2);

  if (event & 0x80000000)
    addy = Kern_cnt::ctr_2_valid(event & 0xff);
  else
    {
      Perf_cnt::split_event(event, &evntsel, &unit_mask);
      addy = Perf_cnt::lookup_event(evntsel);
      if (addy == Perf_cnt::No_event)
        addy = 0;
      else
        addy += add_kcnt;
    }

  if (addy > lines - 1)
    {
      absy += addy - lines + 1;
      addy = lines - 1;
    }

  for (;;)
    {
      unsigned i;

      Jdb::cursor(Tbuf_start_line + 2, 1);
      for (i = 0; i < lines; ++i)
        {
          show_perf_event(absy + i);
          putchar('\n');
        }

      for (; i < Jdb_screen::height() - Tbuf_start_line - 2; ++i)
        puts("\033[K");

      for (bool redraw = false; !redraw;)
        {
          const char *dummy;

          Jdb::cursor(addy + Tbuf_start_line + 2, 1);
          putstr(Jdb::esc_emph);
          show_perf_event(absy + addy);
          putstr("\033[m");
          Jdb::cursor(addy + Tbuf_start_line + 2, 1);
          int c = Jdb_core::getchar();
          show_perf_event(absy + addy);
          if (Jdb::std_cursor_key(c, 0, lines, max_absy, 0,
                                  &absy, &addy, nullptr, &redraw))
            continue;

          switch (c)
            {
            case KEY_RETURN:
            case KEY_RETURN_2:
              absy += addy;
              if (absy < add_kcnt)
                return Kern_cnt::valid_2_ctr(absy) | 0x80000000;

              absy -= add_kcnt;
              Perf_cnt::get_perf_event(absy, &evntsel, &dummy, &dummy);
              unit_mask = select_perf_event_unit_mask(absy, unit_mask);
              if (unit_mask != No_mask)
                {
                  Perf_cnt::combine_event(evntsel, unit_mask, &event);
                  return event;
                }
              [[fallthrough]];
            case KEY_ESC:
              return Perf_cnt::No_event;
            default:
              if (Jdb::is_toplevel_cmd(c))
                return Perf_cnt::No_event;
            }
        }
    }
}

static void
Jdb_tbuf_show::show_events(size_t pos, size_t ref, unsigned count, Unsigned8 mode,
                           Unsigned8 time_mode, bool long_output)
{
  Unsigned64 ref_tsc;
  Unsigned32 ref_kclock, ref_pmc1, ref_pmc2;
  Mword dummy;

  if (ref == Jdb_tbuf::Not_found) [[unlikely]]
    return;

  if (!Jdb_tbuf::event(ref, &dummy, &ref_kclock, &ref_tsc, &ref_pmc1,
                       &ref_pmc2)) [[unlikely]]
    return;

  for (unsigned i = 0; i < count; ++i)
    {
      Tb_sequence seq;  // event number
      Unsigned64 utsc;  // time stamp of this event
      Signed64 dtsc;    // time stamp difference to previous event on same CPU
      Unsigned32 kclock;// kernel clock value of this event
      Signed32 dpmc;    // perf ctr difference (counter 1 or 2) to previous event
      Unsigned32 upmc1; // perf ctr value for counter 1 of this event
      Unsigned32 upmc2; // perf ctr value for counter 2 of this event

      Kconsole::console()->getchar_chance();
      _buffer_str.reset();

      if (!Jdb_tbuf::event(pos, &seq, &kclock, &utsc, &upmc1, &upmc2))
        break;

      if (long_output)
        {
          char s[3] = "  ";
          Jdb_tbuf_output::print_entry(&_buffer_str, pos);

          if (!Jdb_tbuf::diff_tsc(pos, &dtsc))
            dtsc = 0;

          for (unsigned i = 0; i < 10; ++i)
            if (seq == _seq_pos[i])
              {
                s[0] = 'M';
                s[1] = i + '0';
                break;
              }

          // Print time stamp in seconds (s_tsc_ds and s_tsc_ss): It has the
          // form of 'SSS.UUUUUU' with always 6 digits 'U' for microseconds
          // and a variable number of digits 'S' for seconds.
          String_buf<13> s_tsc_dc;
          String_buf<17> s_tsc_ds; // up to 'SSSSSSS.UUUUUU s', > 115.5 days
          String_buf<13> s_tsc_sc;
          String_buf<19> s_tsc_ss; // up to 'SSSSSSSS.UUUUUU s', > 31.7 years
          Jdb::write_ll_dec(&s_tsc_dc, dtsc, false);
          Jdb::write_tsc_s (&s_tsc_ds, dtsc, false);
          Jdb::write_ll_dec(&s_tsc_sc, utsc, false);
          Jdb::write_tsc_s (&s_tsc_ss, utsc, false);

          printf("%-3s%10lu.  %120.120s %12s (%16s)  %12s (%18s) kclk=%u\n",
                 s, seq, _buffer_str.begin() + y_offset, s_tsc_dc.c_str(),
                 s_tsc_ds.c_str(), s_tsc_sc.c_str(), s_tsc_ss.c_str(), kclock);
        }
      else
        {
          String_buf<13> s;
          Jdb_tbuf_output::print_entry(&_buffer_str, pos);
          switch (mode)
            {
            case Index_mode:
              s.printf("%12lu", seq);
              break;
            case Tsc_delta_mode:
              if (!Jdb_tbuf::diff_tsc(pos, &dtsc))
                dtsc = 0;
              switch (time_mode)
                {
                case 0: Jdb::write_ll_hex(&s, dtsc, false); break;
                case 1: Jdb::write_tsc   (&s, dtsc, false); break;
                case 2: Jdb::write_ll_dec(&s, dtsc, false); break;
                }
              break;
            case Tsc_ref_mode:
              dtsc = (pos == ref) ? 0 : utsc - ref_tsc;
              switch (time_mode)
                {
                case 0: Jdb::write_ll_hex(&s, dtsc, true); break;
                case 1: Jdb::write_tsc   (&s, dtsc, true); break;
                case 2: Jdb::write_ll_dec(&s, dtsc, true); break;
                }
              break;
            case Tsc_start_mode:
              dtsc = utsc;
              switch (time_mode)
                {
                case 0: Jdb::write_ll_hex(&s, dtsc, true); break;
                case 1: Jdb::write_tsc   (&s, dtsc, false); break;
                case 2: Jdb::write_ll_dec(&s, dtsc, true); break;
                }
              break;
            case Kclock_ref_mode:
              if (kclock == ref_kclock)
                s.printf("%12d", 0);
              else
                {
                  if (time_mode != 1)
                    Jdb::write_ll_hex(
                      &s, static_cast<Signed64>(kclock) - ref_kclock, true);
                  else
                    s.printf("%+12lld",
                             static_cast<Signed64>(kclock) - ref_kclock);
                }
              break;
            case Kclock_start_mode:
              s.printf(time_mode != 1 ? "%012x" : "%12u", kclock);
              break;
            case Pmc1_delta_mode:
            case Pmc2_delta_mode:
              if (!Jdb_tbuf::diff_pmc(pos, mode - Pmc1_delta_mode, &dpmc))
                dpmc = 0;
              Jdb::write_ll_dec(&s, dpmc, false);
              break;
            case Pmc1_ref_mode:
              dpmc = (pos == ref) ? 0 : upmc1 - ref_pmc1;
              Jdb::write_ll_dec(&s, dpmc, true);
              break;
            case Pmc2_ref_mode:
              dpmc = (pos == ref) ? 0 : upmc2 - ref_pmc2;
              Jdb::write_ll_dec(&s, dpmc, true);
              break;
            }

          const char *c = "";
          if (pos == ref)
            c = Jdb::esc_emph2;
          else
            {
              for (unsigned i = 0; i < 10; ++i)
                if (seq == _seq_pos[i])
                  {
                    c = Jdb::esc_mark;
                    break;
                  }
            }
          printf("%s%-*.*s %12s\033[m%s",
                 c, Jdb_screen::width() - 13, Jdb_screen::width() - 13,
                _buffer_str.begin() + y_offset, s.c_str(),
                count != 1 ? "\n" : "");
        }

      ++pos;
    }
}

// search in tracebuffer
static size_t
Jdb_tbuf_show::search(size_t start, size_t entries, const char *str,
                      Unsigned8 direction)
{
  if (!entries)
    return Jdb_tbuf::Not_found;

  Jdb_regex regex;
  if (!regex.start(str))
    {
      error("Error in regular expression");
      return Jdb_tbuf::Not_found;
    }

  size_t majorant = Jdb_tbuf::entries_majorant();
  size_t found = Jdb_tbuf::Not_found;
  for (size_t pos = ((direction == 1) ? start - 1 : start + 1);;
       (direction == 1) ? --pos : ++pos)
    {
      static String_buf<256> buffer;
      buffer.reset();

      // don't cycle through entries more than once
      // (should not happen due to the following check)
      if (pos == start)
        break;

      // don't wrap around
      if (pos >= majorant)
        {
          error(direction ? "Begin of tracebuffer reached"
                          : "End of tracebuffer reached");
          return found;
        }

      Jdb_tbuf_output::print_entry(&buffer, pos);

      // progress bar
      if ((pos & 0x7f) == 0)
        {
          static int progress;
          Jdb::cursor(Jdb_screen::height(), Jdb_screen::width());
          putchar("|/-\\"[progress++]);
          progress &= 3;
        }

      if (regex.find(buffer.begin(), nullptr, nullptr))
        {
          found = pos;
          break;
        }
      else if (strstr(buffer.begin(), str))
        {
          found = pos;
          break;
        }
    }

  // restore screen
  Jdb::cursor(Jdb_screen::height(), Jdb_screen::width());
  putchar('t');

  return found;
}


static void
Jdb_tbuf_show::find_group(Entry_group *g, Tb_entry const *e, bool older, unsigned lines,
                          unsigned depth)
{
  typedef Entry_group::Group_order Group_order;

  Tb_entry_formatter const *fmt = Tb_entry_formatter::get_fmt(e);
  size_t pos = Jdb_tbuf::pos<Jdb_tbuf::all>(e);
  int dir = older ? 1 : -1;
  while (pos > 0 && !g->full())
    {
      pos += dir;
      Tb_entry *e1 = Jdb_tbuf::lookup<Jdb_tbuf::all>(pos);
      if (!e1)
        return;

      size_t py;
      Group_order t = fmt->is_pair(e, e1);
      if (t.grouped())
        {
          py = Jdb_tbuf::pos(e1);
          if (py < _absy || py >= _absy + lines)
            return;

          if (older && t.depth() >= depth)
            return;
          if (!older && t.depth() <= depth)
            return;

          g->push_back(e1, py, t);

          if (older && t.is_first())
            return;
          if (!older && t.is_last())
            return;

          continue;
        }

      if (!t.is_direct())
        continue;

      Tb_entry_formatter const *fmt2 = Tb_entry_formatter::get_fmt(e1);
      if (fmt2 && fmt2->partner(e1) == e->number())
        {
          py = Jdb_tbuf::pos(e1);
          if (py < _absy || py >= _absy + lines)
            return;
          g->push_back(e1, py, older ? Group_order::first() : Group_order::last());
          // HM: is one direct enry enough
          return;
        }
    }
}

static void
Jdb_tbuf_show::show()
{
  static Unsigned8 mode = Index_mode;
  static Unsigned8 time_mode = 1;
  static Unsigned8 direction = 0;

  size_t entries;
  Jdb_tbuf_output::set_filter(_filter_str, &entries);
  Jdb::clear_screen();

restart:
  unsigned lines = Jdb_screen::height() - 4;

  if (Jdb_tbuf::capacity() < lines)
    lines = Jdb_tbuf::capacity();

  if (entries < lines)
    lines = entries;

  if (lines < 1)
    lines = 1;

  size_t max_absy = entries > lines ? entries - lines : 0;

  // Search for reference element position. If not found, use last entry.
  size_t refy = Jdb_tbuf::search_to_pos(_seq_ref);
  if (refy == Jdb_tbuf::Hidden || refy == Jdb_tbuf::Not_found)
    {
      if (entries)
        refy = entries - 1;
      else
        refy = Jdb_tbuf::Not_found;
    }

  size_t posy[10];  // Position of mark{0..9}
  // Search mark {0..9}. If not found, set Nil.
  for (unsigned i = 0; i < 10; ++i)
    posy[i] = Jdb_tbuf::search_to_pos(_seq_pos[i]);

  // Search for current cursor position (starting from the top of the screen).
  // If beyond buffer, goto first entry.
  Mword addy = Jdb_tbuf::search_to_pos(_seq_cur);
  if (addy == Jdb_tbuf::Hidden || addy == Jdb_tbuf::Not_found)
    {
      addy = 0;
      _absy = 0;
    }
  else
    addy -= _absy;

  if (addy >= lines - 1)
    {
      addy = 0;
      _absy = 0;
    }

  for (;;)
    {
      Mword count, perf_event[2] = { 0, 0 };
      Mword perf_user[2] = { 0, 0 };
      Mword perf_kern[2] = { 0, 0 };
      Mword perf_edge[2] = { 0, 0 };
      const char *perf_mode[2], *perf_name[2];

      for (unsigned i = 0; i < 2; ++i)
        if (Kern_cnt::mode(i, &perf_mode[i], &perf_name[i], &perf_event[i]) ||
            Perf_cnt::mode(i, &perf_mode[i], &perf_name[i], &perf_event[i],
                           &perf_user[i], &perf_kern[i], &perf_edge[i]))
          {}

      static const char * const mode_str[] =
        { "index", "tsc diff", "tsc rel", "tsc start", "kclock rel",
          "kclock", "pmc1 diff", "pmc2 diff", "pmc1 rel", "pmc2 rel" };

      const char *perf_type = Perf_cnt::perf_type();

      Jdb::cursor();
      printf("%3zu%% of %-6zu Perf:%-4s 1=" L4_PTR_FMT
             "(%s%s\033[m%s%s%s\033[m)\033[K",
             Jdb_tbuf::entries_majorant() * 100 / Jdb_tbuf::capacity(),
             Jdb_tbuf::capacity(),
             perf_type, perf_event[0], Jdb::esc_emph, perf_mode[0],
             perf_name[0] && *perf_name[0] ? ":" : "",
             mode==Pmc1_delta_mode || mode==Pmc1_ref_mode ? Jdb::esc_emph : "",
             perf_name[0]);

      Jdb::cursor(1, Jdb_screen::width() - 9);
      printf("%10s\n"
             "%24s 2=" L4_PTR_FMT "(%s%s\033[m%s%s%s\033[m)\033[K\n",
              mode_str[mode], "",
              perf_event[1], Jdb::esc_emph, perf_mode[1],
              perf_name[1] && *perf_name[1] ? ":" : "",
              mode==Pmc2_delta_mode || mode==Pmc2_ref_mode ? Jdb::esc_emph : "",
              perf_name[1]);

      if (_filter_str[0])
        {
          Jdb::cursor(2, 1);
          printf("\033[31m%3zu%% filtered\033[m\n",
                 entries * 100U / Jdb_tbuf::capacity());
        }

      for (unsigned i = 3; i < Tbuf_start_line; ++i)
        puts("\033[K");

      show_events(_absy, refy, lines, mode, time_mode, false);
      if (lines == 1)
        puts("\033[K");

      for (unsigned i = Tbuf_start_line + lines; i < Jdb_screen::height(); ++i)
        puts("\033[K");

      _status_type = Status_redraw;

    status_line:
      for (bool redraw = false; !redraw;)
        {
          Smword c;
          Unsigned8 d = 0; // default search direction is forward

          if (_status_type == Status_redraw)
            {
              Jdb::printf_statline("tbuf", "/?nN=search sj=mark c=clear r=ref "
                                   "F=filter D=dump P=perf <CR>=select", "_");
              _status_type = Status_ok;
            }
          else if (_status_type == Status_error)
            _status_type = Status_redraw;

          Entry_group group;

          if (!_filter_str[0])
            {
              typedef Tb_entry::Group_order Group_order;

              Group_order pt;
              Tb_entry const *ce = Jdb_tbuf::lookup(_absy + addy);

              if (ce)
                {
                  Tb_entry_formatter const *fmt = Tb_entry_formatter::get_fmt(ce);

                  if (fmt)
                    pt = fmt->has_partner(ce);

                  if (!pt.not_grouped())
                    {
                      if (!pt.is_first())
                        find_group(&group, ce, true, lines, pt.depth());
                      if (!pt.is_last())
                        find_group(&group, ce, false, lines, pt.depth());
                    }

                  for (unsigned i = 0; i < group.size(); ++i)
                    {
                      Entry_group::Item const &item = group[i];
                      Jdb::cursor(item.y - _absy + Tbuf_start_line, 3);
                      putstr(Jdb::esc_emph);
                      if (item.order.is_first())
                        putstr("<++");
                      else if (item.order.is_last())
                        putstr("++>");
                      else if (item.order.grouped())
                        putstr("+++");
                      putstr("\033[m");
                    }
                }
            }

          Jdb::cursor(addy + Tbuf_start_line, 1);
          putstr(Jdb::esc_emph);
          show_events(_absy + addy, refy, 1, mode, time_mode, false);
          putstr("\033[m");
          Jdb::cursor(addy + Tbuf_start_line, 1);

          // WAIT for key.....
          c = Jdb_core::getchar();

          show_events(_absy + addy, refy, 1, mode, time_mode, false);
          for (unsigned i = 0; i < group.size(); ++i)
            {
              Entry_group::Item const &item = group[i];
              Jdb::cursor(item.y - _absy + Tbuf_start_line, 1);
              show_events(item.y, refy, 1, mode, time_mode, false);
            }

          if (Jdb::std_cursor_key(c, 0, lines, max_absy, 0,
                                  &_absy, &addy, nullptr, &redraw))
            continue;

          size_t pos;
          switch (c)
            {
            case 'h':
              if (y_offset > 10)
                y_offset -= 10;
              else
                y_offset = 0;

              redraw = true;
              break;
            case 'l':
              if (y_offset < sizeof(_buffer_str) - 82)
                y_offset += 10;
              else
                y_offset = sizeof(_buffer_str) - 72;

              redraw = true;
              break;
            case 'F': // filter view by regex
              Jdb::printf_statline("tbuf", nullptr, "Filter(%s)=%s",
                                   Jdb_regex::avail() ? "regex" : "instr",
                                   _filter_str);
              _status_type = Status_redraw;
              Jdb::cursor(Jdb_screen::height(), 21 + strlen(_filter_str));
              if (!Jdb_input::get_string(_filter_str, sizeof(_filter_str)))
                goto status_line;
              if (!Jdb_tbuf_output::set_filter(_filter_str, &entries))
                {
                  error("Error in regular expression");
                  goto status_line;
                }

              _absy = 0;
              _seq_cur = Jdb_tbuf::Nil;
              goto restart;
            case 'D': // dump to console
              if (!Kconsole::console()->find_console(Console::GZIP))
                break;

              Jdb::cursor(Jdb_screen::height(), 10);
              Jdb::clear_to_eol();
              printf("Count=");
              if (Jdb_input::get_mword(&count, 7, 10))
                {
                  if (count == 0)
                    count = ~0UL;
                  Kconsole::console()->start_exclusive(Console::GZIP);
                  show_events(_absy, refy, count, mode, time_mode, true);
                  Kconsole::console()->end_exclusive(Console::GZIP);
                  Jdb::clear_screen();
                  redraw = true;
                  break;
                }

              _status_type = Status_redraw;
              goto status_line;
            case 'P': // set performance counter
                {
                  Mword event, e;
                  int user, kern, edge, nr;

                  Jdb::printf_statline("tbuf", "1..2=select counter", "P");
                  Jdb::cursor(Jdb_screen::height(), 8);
                  for (;;)
                    {
                      nr = Jdb_core::getchar();
                      if (nr == KEY_ESC)
                        goto exit;
                      if (nr >= '1' && nr <= '2')
                        {
                          nr -= '1';
                          break;
                        }
                    }

                  event = perf_event[nr];
                  user  = perf_user[nr];
                  kern  = perf_kern[nr];
                  edge  = perf_edge[nr];
                  Jdb::printf_statline("tbuf", "d=duration e=edge u=user "
                                       "k=kern +=both -=none ?=event",
                                       "P%c", '1' + nr);

                  Jdb::cursor(Jdb_screen::height(), 9);
                  switch (c = Jdb_core::getchar())
                    {
                    case '+': user = 1; kern = 1;            break;
                    case '-': user = 0; kern = 0; event = 0; break;
                    case 'u': user = 1; kern = 0;            break;
                    case 'k': user = 0; kern = 1;            break;
                    case 'd': edge = 0;                      break;
                    case 'e': edge = 1;                      break;
                    case '?':
                    case '_':
                      if ((e = select_perf_event(event)) == Perf_cnt::No_event)
                        {
                          redraw = true;
                          break;
                        }
                      event = e;
                      redraw = true;
                      break;
                    default:  Jdb_input::get_mword(&event, 4, 16, c);
                              break;
                    }

                  if (!Kern_cnt::setup_pmc(nr, event) &&
                      !Perf_cnt::setup_pmc(nr, event, user, kern, edge))
                    Tb_entry::set_rdcnt(nr, nullptr);

                  redraw = true;
                }
              break;
            case KEY_RETURN: // disassemble eip of current entry
            case KEY_RETURN_2:
                {
                  Thread const *t = nullptr;
                  Mword eip;
                  if (Jdb_tbuf_output::thread_ip(_absy + addy, &t, &eip))
                    {
                      if (Jdb_disasm::avail())
                        {
                          if (!Jdb_disasm::show(Jdb_address(eip, t->space()), 1))
                            goto exit;
                        }
                      else
                        { // hacky, we should get disasm working for arm too
                          // (at least without the disassembly itself)
                          printf("\n==========================\n"
                                 "   ip=%lx                 \n"
                                 "==========================\n", eip);
                          Jdb_core::getchar();
                        }
                      redraw = true;
                    }
                }
              break;
            case KEY_TAB:
              Jdb_tbuf_output::toggle_names();
              redraw = true;
              break;
            case KEY_CURSOR_LEFT: // mode switch
              if (mode == Index_mode)
                mode = Pmc2_ref_mode;
              else
                mode--;
              redraw = true;
              break;
            case KEY_CURSOR_RIGHT: // mode switch
              mode++;
              if (mode > Pmc2_ref_mode)
                mode = Index_mode;
              redraw = true;
              break;
            case ' ': // mode switch
              if (mode != Index_mode)
                {
                  time_mode = (time_mode + 1) % 3U;
                  redraw = true;
                }
              break;
            case 'c': // clear tracebuffer
              Jdb_tbuf::clear_tbuf();
              _absy = 0;
              _seq_cur = Jdb_tbuf::Nil;
              _seq_ref = Jdb_tbuf::Nil;
              entries = 0;

              for (unsigned i = 0; i < 10; ++i)
                _seq_pos[i] = Jdb_tbuf::Nil;

              goto restart;
            case 's': // set mark
              Jdb::printf_statline("tbuf", nullptr, "set mark [0-9] ");
              _status_type = Status_redraw;
              c = Jdb_core::getchar();
              if (!entries || c < '0' || c > '9')
                {
                  error("Invalid marker");
                  goto status_line;
                }

                {
                  unsigned i = c - '0';
                  posy[i] = _absy + addy;
                  _seq_pos[i] = Jdb_tbuf::lookup(_absy + addy)->number();
                }

              redraw = true;
              break;
            case 'r': // set reference entry
              if (!entries)
                break;

              refy = _absy + addy;
              _seq_ref = Jdb_tbuf::lookup(_absy + addy)->number();
              redraw = true;
              break;
            case 'j': // jump to mark or reference element
              Jdb::printf_statline("tbuf", nullptr, "jump to mark [0-9] or ref [r] ");
              _status_type = Status_redraw;
              c = Jdb_core::getchar();
              if ((c < '0' || c > '9') && c != 'r')
                {
                  error("Invalid marker");
                  goto status_line;
                }

              pos = (c == 'r') ? refy : posy[c - '0'];
              if (pos == Jdb_tbuf::Not_found)
                {
                  error("Mark unset");
                  goto status_line;
                }

              if (pos == Jdb_tbuf::Hidden)
                {
                  error("Mark not visible within current filter");
                  goto status_line;
                }

              goto jump_index;
            case '?': // search backward
              d = 1;
              [[fallthrough]];
            case '/': // search forward
              direction = d;
              // search in tracebuffer events
              Jdb::printf_statline("tbuf", nullptr, "%s=%s",
                                   Jdb_regex::avail() ? "Regexp" : "Search",
                                   _search_str);
              _status_type = Status_redraw;
              Jdb::cursor(Jdb_screen::height(), 14 + strlen(_search_str));
              if (!Jdb_input::get_string(_search_str, sizeof(_search_str)) ||
                  !_search_str[0])
                goto status_line;
              [[fallthrough]];
            case 'n': // search next
            case 'N': // search next reverse
              pos = search(_absy + addy, entries, _search_str,
                           c == 'N' ? !direction : direction);
              if (pos != Jdb_tbuf::Not_found)
                {
                  // found
                jump_index:
                  if (pos < _absy || pos > _absy + lines - 1)
                    {
                      // screen crossed
                      addy = 4;
                      _absy = pos - addy;

                      if (pos < addy)
                        {
                          addy = pos;
                          _absy = 0;
                        }

                      if (_absy > max_absy)
                        {
                          _absy = max_absy;
                          addy = pos - _absy;
                        }

                      redraw = true;
                      break;
                    }
                  else
                    addy = pos - _absy;
                }

              goto status_line;
            case KEY_ESC:
              Jdb::abort_command();
              goto exit;
            default:
              if (Jdb::is_toplevel_cmd(c))
                goto exit;
            }
        }
    }

exit:
  auto e = Jdb_tbuf::lookup(_absy + addy);
  _seq_cur = e ? e->number() : Jdb_tbuf::Nil;
}

PUBLIC
Jdb_module::Action_code
Jdb_tbuf_show::action(int cmd, void *&, char const *&, int &) override
{
  switch (cmd)
    {
    case 0:
      show();
      break;

    case 1:
      Jdb_tbuf_output::set_filter(_filter_str, nullptr);
      show_events(0, 0, 1000000, 0, 0, true);
      break;

    case 2:
      if (Kconsole::console()->find_console(Console::GZIP))
        {
          Jdb_tbuf_output::set_filter(_filter_str, nullptr);
          Kconsole::console()->start_exclusive(Console::GZIP);
          show_events(0, 0, 1000000, 0, 0, true);
          Kconsole::console()->end_exclusive(Console::GZIP);
        }
      break;
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_tbuf_show::cmds() const override
{
  static Cmd cs[] =
    {
        { 0, "T", "tbuf", "",
          "T{P{+|-|k|u|<event>}}\tenter tracebuffer, on/off/kernel/user perf",
          nullptr },
        { 1, "", "tbufdumptext", "", nullptr /* invisible */, nullptr },
        { 2, "", "tbufdumpgzip", "", nullptr /* invisible */, nullptr },
    };

  return cs;
}

PUBLIC
int
Jdb_tbuf_show::num_cmds() const override
{
  return 3;
}

IMPLEMENT
Jdb_tbuf_show::Jdb_tbuf_show()
    : Jdb_module("MONITORING")
{}

static Jdb_tbuf_show jdb_tbuf_show INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
