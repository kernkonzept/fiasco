IMPLEMENTATION:

#include <cstdio>
#include <cstring>

#include "config.h"
#include "cpu.h"
#include "jdb.h"
#include "jdb_disasm.h"
#include "jdb_input.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_symbol.h"
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
    Mword y;
    Group_order order;
    Item() : e(0), y(0), order(Group_order::none()) {}
  };

  Entry_group() : _c(0) {}

  Item const &operator [] (unsigned i) const { return _i[i]; }
  Item &operator [] (unsigned i) { return _i[i]; }
  unsigned size() const { return _c; }
  bool full() const { return _c >= Max_group_size; }
  unsigned push_back(Tb_entry const *e, Mword y, Group_order t)
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
  static Mword _status_type;
  static Mword _absy;
  static Mword _nr_cur;
  static Mword _nr_ref;
  static Mword _nr_pos[10];
  static Mword y_offset;

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

  enum
  {
    Status_ok         = 0,
    Status_redraw     = 1,
    Status_error      = 2,
  };

  enum
  {
    Nil               = (Mword)-1,
  };
};

char  Jdb_tbuf_show::_search_str[40];
char  Jdb_tbuf_show::_filter_str[40];
String_buf<512> Jdb_tbuf_show::_buffer_str;
Mword Jdb_tbuf_show::_status_type;
Mword Jdb_tbuf_show::_absy;
Mword Jdb_tbuf_show::_nr_cur;
Mword Jdb_tbuf_show::_nr_ref;
Mword Jdb_tbuf_show::_nr_pos[10] = { Nil, Nil, Nil, Nil, Nil,
                                     Nil, Nil, Nil, Nil, Nil };

Mword Jdb_tbuf_show::y_offset = 0;

static void
Jdb_tbuf_show::error(const char * const msg)
{
  Jdb::printf_statline("tbuf", 0, "\033[31;1m=== %s! ===\033[m", msg);
  _status_type = Status_error;
}

static void
Jdb_tbuf_show::show_perf_event_unit_mask_entry(Mword nr, Mword idx,
                                               Mword unit_mask, int exclusive)
{
  const char *desc;
  Mword value;

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
  const char *name, *desc;
  unsigned evntsel;
  Mword add_kcnt = Config::Jdb_accounting ? Kern_cnt_max : 0;

  if (nr < add_kcnt)
    {
      const char * const s = Kern_cnt::get_str(nr);
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
  Mword absy     = 0;
  Mword addy     = 0;
  Mword max_absy = 0;
  Mword lines    = 10;
  Mword cols	 = Jdb_screen::cols() - 1;

  Mword default_value, nvalues, value;
  Perf_cnt::Unit_mask_type type;
  const char *desc;

  Perf_cnt::get_unit_mask(nr, &type, &default_value, &nvalues);
  int  exclusive = (type == Perf_cnt::Exclusive);

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
  show_perf_event(nr + (Config::Jdb_accounting ? Kern_cnt_max : 0));
  printf("\033[m\033[K\n"
         "\033[K\n"
         "  \033[1;32mSelect Event Mask (%s):\033[m\033[K\n"
         "\033[K\n", exclusive ? "exclusive" : "bitmap");

  for (;;)
    {
      Mword i;

      Jdb::cursor(Tbuf_start_line + 4, 1);
      for (i = 0; i < lines; i++)
        {
          show_perf_event_unit_mask_entry(nr, i, unit_mask, exclusive);
          putchar('\n');
        }
      for (; i<Jdb_screen::height() - Tbuf_start_line - 5; i++)
        puts("\033[K");

      for (bool redraw = false; !redraw; )
        {
          int c;
          const char *dummy;
          Mword value;

          Jdb::cursor(addy+Tbuf_start_line + 4, 1);
          putstr(Jdb::esc_emph);
          show_perf_event_unit_mask_entry(nr, absy + addy, unit_mask, exclusive);
          putstr("\033[m");
          Jdb::cursor(addy+Tbuf_start_line + 4, 1);
          c = Jdb_core::getchar();
          show_perf_event_unit_mask_entry(nr, absy + addy, unit_mask, exclusive);
          Perf_cnt::get_unit_mask_entry(nr, absy + addy, &value, &dummy);

          if (Jdb::std_cursor_key(c, cols, lines, max_absy,
                                  &absy, &addy, 0, &redraw))
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
              return Nil;
            default:
              if (Jdb::is_toplevel_cmd(c))
                return Nil;
            }
        }
    }
}

static Mword
Jdb_tbuf_show::select_perf_event(Mword event)
{
  Mword absy     = 0;
  Mword addy     = 0;
  Mword add_kcnt = Config::Jdb_accounting ? Kern_cnt_max : 0;
  Mword nevents  = Perf_cnt::get_max_perf_event() + add_kcnt;
  Mword lines    = (nevents < Jdb_screen::height() - 6)
                   ? nevents
                   : Jdb_screen::height() - 6;
  Mword cols     = Jdb_screen::cols() - 1;
  Mword max_absy = nevents-lines;

  if (nevents == 0)
    // libperfctr not linked
    return Nil;

  unsigned evntsel;
  Mword unit_mask;

  Jdb::printf_statline("tbuf", "<CR>=select", "P?");

  Jdb::cursor(Tbuf_start_line, 1);
  printf("%sSelect Performance Counter\033[m\033[K\n\033[K", Jdb::esc_emph2);

  if (event & 0x80000000)
    addy = event & 0xff;
  else
    {
      Perf_cnt::split_event(event, &evntsel, &unit_mask);
      addy = Perf_cnt::lookup_event(evntsel);
      if (addy == Nil)
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
      Mword i;

      Jdb::cursor(Tbuf_start_line + 2, 1);
      for (i = 0; i < lines; i++)
        {
          show_perf_event(absy+i);
          putchar('\n');
        }
      for (; i<Jdb_screen::height() - Tbuf_start_line - 2; i++)
        puts("\033[K");

      for (bool redraw = false; !redraw; )
        {
          const char *dummy;

          Jdb::cursor(addy + Tbuf_start_line + 2, 1);
          putstr(Jdb::esc_emph);
          show_perf_event(absy+addy);
          putstr("\033[m");
          Jdb::cursor(addy + Tbuf_start_line + 2, 1);
          int c = Jdb_core::getchar();
          show_perf_event(absy + addy);
          if (Jdb::std_cursor_key(c, cols, lines, max_absy,
                                  &absy, &addy, 0, &redraw))
            continue;

          switch (c)
            {
            case KEY_RETURN:
            case KEY_RETURN_2:
              absy += addy;
              if (absy < add_kcnt)
                return absy | 0x80000000;

              absy -= add_kcnt;
              Perf_cnt::get_perf_event(absy, &evntsel, &dummy, &dummy);
              unit_mask = select_perf_event_unit_mask(absy, unit_mask);
              if (unit_mask != Nil)
                {
                  Perf_cnt::combine_event(evntsel, unit_mask, &event);
                  return event;
                }
              // FALLTHRU
            case KEY_ESC:
              return Nil;
            default:
              if (Jdb::is_toplevel_cmd(c))
                return Nil;
            }
        }
    }
}


static void
Jdb_tbuf_show::show_events(Mword n, Mword ref, Mword count, Unsigned8 mode,
                           Unsigned8 time_mode, int long_output)
{
  Unsigned64 ref_tsc;
  Unsigned32 ref_kclock, ref_pmc1, ref_pmc2;
  Mword dummy, i;

  Jdb_tbuf::event(ref, &dummy, &ref_kclock, &ref_tsc, &ref_pmc1, &ref_pmc2);

  for (i = 0; i < count; i++)
    {
      Mword number;
      Signed64 dtsc;
      Signed32 dpmc;
      Unsigned64 utsc;
      Unsigned32 kclock, upmc1, upmc2;

      Kconsole::console()->getchar_chance();
      _buffer_str.reset();

      if (!Jdb_tbuf::event(n, &number, &kclock, &utsc, &upmc1, &upmc2))
        break;

      if (long_output)
        {
          char s[3];
          Jdb_tbuf_output::print_entry(&_buffer_str, n);

          if (!Jdb_tbuf::diff_tsc(n, &dtsc))
            dtsc = 0;

          strcpy(s, "  ");
          if (n == ref)
            {
              s[0] = 'R';
              s[1] = ' ';
            }
          else
            {
              for (int i = 0; i < 10; i++)
                if (number == _nr_pos[i])
                  {
                    s[0] = 'M';
                    s[1] = i + '0';
                    break;
                  }
            }

          String_buf<13> s_tsc_dc;
          String_buf<15> s_tsc_ds;
          String_buf<13> s_tsc_sc;
          String_buf<15> s_tsc_ss;
          Jdb::write_ll_dec(&s_tsc_dc, dtsc, false); s_tsc_dc.terminate();
          Jdb::write_tsc_s (&s_tsc_ds, dtsc, false); s_tsc_ds.terminate();
          Jdb::write_ll_dec(&s_tsc_sc, utsc, false); s_tsc_sc.terminate();
          Jdb::write_tsc_s (&s_tsc_ss, utsc, false); s_tsc_ss.terminate();

          printf("%-3s%10lu.  %120.120s %13.13s (%14.14s)  %13.13s (%14.14s) kclk=%u\n",
                 s, number, _buffer_str.begin() + y_offset, s_tsc_dc.begin(), s_tsc_ds.begin(),
                 s_tsc_sc.begin(), s_tsc_ss.begin(), kclock);
        }
      else
        {
          String_buf<13> s;
          Jdb_tbuf_output::print_entry(&_buffer_str, n);
          switch (mode)
            {
            case Index_mode:
              s.printf("%12lu", number);
              break;
            case Tsc_delta_mode:
              if (!Jdb_tbuf::diff_tsc(n, &dtsc))
                dtsc = 0;
              switch (time_mode)
                {
                case 0: Jdb::write_ll_hex(&s, dtsc, false); break;
                case 1: Jdb::write_tsc   (&s, dtsc, false); break;
                case 2: Jdb::write_ll_dec(&s, dtsc, false); break;
                }
              break;
            case Tsc_ref_mode:
              dtsc = (n == ref) ? 0 : utsc - ref_tsc;
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
                    Jdb::write_ll_hex(&s, (Unsigned64)kclock - ref_kclock, true);
                  else
                    s.printf("%+12d", (int)kclock - (int)ref_kclock);
                }
              break;
            case Kclock_start_mode:
              s.printf(time_mode != 1 ? "%012x" : "%12u", kclock);
              break;
            case Pmc1_delta_mode:
            case Pmc2_delta_mode:
              if (!Jdb_tbuf::diff_pmc(n, mode - Pmc1_delta_mode, &dpmc))
                dpmc = 0;
              Jdb::write_ll_dec(&s, (Signed64)dpmc, false);
              break;
            case Pmc1_ref_mode:
              dpmc = (n == ref) ? 0 : upmc1 - ref_pmc1;
              Jdb::write_ll_dec(&s, (Signed64)dpmc, true);
              break;
            case Pmc2_ref_mode:
              dpmc = (n == ref) ? 0 : upmc2 - ref_pmc2;
              Jdb::write_ll_dec(&s, (Signed64)dpmc, true);
              break;
            }

          const char *c = "";
          if (n == ref)
            c = Jdb::esc_emph2;
          else
            {
              for (int i = 0; i < 10; i++)
                if (number == _nr_pos[i])
                  {
                    c = Jdb::esc_mark;
                    break;
                  }
            }
          printf("%s%-*.*s %12s\033[m%s",
                 c, (int)Jdb_screen::width() - 13, (int)Jdb_screen::width() - 13,
                _buffer_str.begin() + y_offset, s.begin(),
                count != 1 ? "\n" : "");
        }
       n++;
    }
}

// search in tracebuffer
static Mword
Jdb_tbuf_show::search(Mword start, Mword entries, const char *str,
                      Unsigned8 direction)
{
  Mword found = Nil;

  if (!entries)
    return found;

  if (Jdb_regex::avail() && !Jdb_regex::start(str))
    {
      error("Error in regular expression");
      return found;
    }

  for (Mword n=direction==1 ? start-1 : start+1; ; (direction==1) ? n-- : n++)
    {
      static String_buf<256> buffer;
      buffer.reset();

      // don't cycle through entries more than once
      // (should not happen due to the following check)
      if (n == start)
        break;
      // don't wrap around
      if (!Jdb_tbuf::event_valid(n))
        {
          error(direction ? "Begin of tracebuffer reached"
                          : "End of tracebuffer reached");
          return found;
        }

      if (!Jdb_tbuf::event_valid(n))
        n = (direction==1) ? entries-1 : 0;

      Jdb_tbuf_output::print_entry(&buffer, n);

      // progress bar
      if ((n & 0x7f) == 0)
        {
          static int progress;
          Jdb::cursor(Jdb_screen::height(), 79);
          putchar("|/-\\"[progress++]);
          progress &= 3;
        }

      if (Jdb_regex::avail() && Jdb_regex::find(buffer.begin(), 0, 0))
        {
          found = n;
          break;
        }
      else if (strstr(buffer.begin(), str))
        {
          found = n;
          break;
        }
    }

  // restore screen
  Jdb::cursor(Jdb_screen::height(), 79);
  putchar('t');

  return found;
}


static void
Jdb_tbuf_show::find_group(Entry_group *g, Tb_entry const *e, bool older, unsigned lines,
                          unsigned depth)
{
  typedef Entry_group::Group_order Group_order;

  Tb_entry_formatter const *fmt = Tb_entry_formatter::get_fmt(e);
  Mword idx = Jdb_tbuf::unfiltered_idx(e);
  int dir = older ? 1 : -1;
  while (idx > 0 && !g->full())
    {
      idx += dir;
      Tb_entry *e1 = Jdb_tbuf::unfiltered_lookup(idx);
      if (!e1)
        return;

      Mword py;
      Group_order t = fmt->is_pair(e, e1);
      if (t.grouped())
        {
          py = Jdb_tbuf::idx(e1);
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
          py = Jdb_tbuf::idx(e1);
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
  static Unsigned8 mode      = Index_mode;
  static Unsigned8 time_mode = 1;
  static Unsigned8 direction = 0;
  Mword  entries;

  Jdb_tbuf_output::set_filter(_filter_str, &entries);
  Jdb::clear_screen();

restart:
  Mword refy;                    // idx of reference event
  Mword posy[10];                // idx of mark{0..9}
  Mword addy;			 // cursor position starting from top of screen
  Mword lines = Jdb_screen::height()-4;
  Mword cols  = Jdb_screen::cols() - 1;
  Mword n;
  Tb_entry *e;

  if (Jdb_tbuf::max_entries() < lines)
    lines = Jdb_tbuf::max_entries();
  if (entries < lines)
    lines = entries;
  if (lines < 1)
    lines = 1;

  Mword max_absy = entries > lines ? entries - lines : 0;

  // Search reference element. If not found, use last entry.
  if ((refy = Jdb_tbuf::search_to_idx(_nr_ref)) >= (Mword)-2)
    {
      if (entries)
        refy = entries - 1;
      else
        refy = Nil;
    }

  // Search mark {0..9}. If not found, set Nil.
  for (n=0; n<10; n++)
    posy[n] = Jdb_tbuf::search_to_idx(_nr_pos[n]);

  // Search current position. If beyond buffer, goto first entry.
  if ((addy = Jdb_tbuf::search_to_idx(_nr_cur)))
    addy -= _absy;
  else
    addy = _absy = 0;
  if (addy >= lines-1)
    addy = _absy = 0;

  for (;;)
    {
      Mword count, perf_event[2];
      Mword perf_user[2] = { 0, 0 };
      Mword perf_kern[2] = { 0, 0 };
      Mword perf_edge[2] = { 0, 0 };
      const char *perf_mode[2], *perf_name[2];

      for (Mword i = 0; i < 2; i++)
        if (Kern_cnt::mode(i, &perf_mode[i], &perf_name[i], &perf_event[i]) ||
            Perf_cnt::mode(i, &perf_mode[i], &perf_name[i], &perf_event[i],
                           &perf_user[i], &perf_kern[i], &perf_edge[i]))
          {}

      static const char * const mode_str[] =
        { "index", "tsc diff", "tsc rel", "tsc start", "kclock rel",
          "kclock", "pmc1 diff", "pmc2 diff", "pmc1 rel", "pmc2 rel" };

      const char *perf_type = Perf_cnt::perf_type();

      Jdb::cursor();
      printf("%3lu%% of %-6lu Perf:%-4s 1=" L4_PTR_FMT
             "(%s%s\033[m%s%s%s\033[m)\033[K",
             Jdb_tbuf::unfiltered_entries() * 100 / Jdb_tbuf::max_entries(),
             Jdb_tbuf::max_entries(),
             perf_type, perf_event[0], Jdb::esc_emph, perf_mode[0],
             perf_name[0] && *perf_name[0] ? ":" : "",
             mode==Pmc1_delta_mode || mode==Pmc1_ref_mode ? Jdb::esc_emph : "",
             perf_name[0]);

      Jdb::cursor(1, 71);
      printf("%10s\n"
             "%24s 2=" L4_PTR_FMT "(%s%s\033[m%s%s%s\033[m)\033[K\n",
              mode_str[(int)mode], "",
              perf_event[1], Jdb::esc_emph, perf_mode[1],
              perf_name[1] && *perf_name[1] ? ":" : "",
              mode==Pmc2_delta_mode || mode==Pmc2_ref_mode ? Jdb::esc_emph : "",
              perf_name[1]);
      if (_filter_str[0])
        {
          Jdb::cursor(2, 1);
          printf("\033[31m%3lu%% filtered\033[m\n",
                 entries * 100 / Jdb_tbuf::max_entries());
        }
      for (Mword i = 3; i < Tbuf_start_line; i++)
        puts("\033[K");

      show_events(_absy, refy, lines, mode, time_mode, 0);
      if (lines == 1)
        puts("\033[K");

      for (Mword i = Tbuf_start_line + lines; i < Jdb_screen::height(); i++)
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
          show_events(_absy + addy, refy, 1, mode, time_mode, 0);
          putstr("\033[m");
          Jdb::cursor(addy + Tbuf_start_line, 1);

          // WAIT for key.....
          c = Jdb_core::getchar();

          show_events(_absy + addy, refy, 1, mode, time_mode, 0);
          for (unsigned i = 0; i < group.size(); ++i)
            {
              Entry_group::Item const &item = group[i];
              Jdb::cursor(item.y - _absy + Tbuf_start_line, 1);
              show_events(item.y, refy, 1, mode, time_mode, 0);
            }

          if (Jdb::std_cursor_key(c, cols, lines, max_absy,
                                  &_absy, &addy, 0, &redraw))
            continue;

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
                y_offset = sizeof(_buffer_str)-72;

              redraw = true;
              break;
            case 'F': // filter view by regex
              Jdb::printf_statline("tbuf", 0, "Filter(%s)=%s",
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
              _absy   = 0;
              _nr_cur = Nil;
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
                    count = lines;
                  Kconsole::console()->start_exclusive(Console::GZIP);
                  show_events(_absy, refy, count, mode, time_mode, 1);
                  Kconsole::console()->end_exclusive(Console::GZIP);
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
                                       "P%c", (char)'1' + nr);

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
                      if ((e = select_perf_event(event)) == Nil)
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
                    Tb_entry::set_rdcnt(nr, 0);

                  redraw = true;
                }
              break;
            case KEY_RETURN: // disassemble eip of current entry
            case KEY_RETURN_2:
                {
                  Thread const *t = 0;
                  Mword eip;
                  if (Jdb_tbuf_output::thread_ip(_absy+addy, &t, &eip))
                    {
                      if (Jdb_disasm::avail())
                        {
                          if (!Jdb_disasm::show(eip, t->space(), 1, 1))
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
                  time_mode = (time_mode + 1) % 3;
                  redraw = true;
                }
              break;
            case 'c': // clear tracebuffer
              Jdb_tbuf::clear_tbuf();
              _absy   = 0;
              _nr_cur = Nil;
              _nr_ref = Nil;
              entries = 0;
              for (n = 0; n < 10; n++)
                _nr_pos[n] = Nil;
              goto restart;
            case 's': // set mark
              Jdb::printf_statline("tbuf", 0, "set mark [0-9] ");
              _status_type = Status_redraw;
              c = Jdb_core::getchar();
              if (!entries || c < '0' || c > '9')
                {
                  error("Invalid marker");
                  goto status_line;
                }
              n = c - '0';
              posy[n]    = _absy + addy;
              _nr_pos[n] = Jdb_tbuf::lookup(_absy+addy)->number();
              redraw     = true;
              break;
            case 'r': // set reference entry
              if (!entries)
                break;
              refy       = _absy + addy;
              _nr_ref    = Jdb_tbuf::lookup(_absy+addy)->number();
              redraw     = true;
              break;
            case 'j': // jump to mark or reference element
              Jdb::printf_statline("tbuf", 0, "jump to mark [0-9] or ref [r] ");
              _status_type = Status_redraw;
              c = Jdb_core::getchar();
              if ((c < '0' || c > '9') && c != 'r')
                {
                  error("Invalid marker");
                  goto status_line;
                }
              n = (c == 'r') ? refy : posy[c-'0'];
              if (n == Nil)
                {
                  error("Mark unset");
                  goto status_line;
                }
              else if (n == (Mword)-2)
                {
                  error("Mark not visible within current filter");
                  goto status_line;
                }
              goto jump_index;
            case '?': // search backward
              d = 1;
              // fall through
            case '/': // search forward
              direction = d;
              // search in tracebuffer events
              Jdb::printf_statline("tbuf", 0, "%s=%s",
                                   Jdb_regex::avail() ? "Regexp" : "Search",
                                   _search_str);
              _status_type = Status_redraw;
              Jdb::cursor(Jdb_screen::height(), 14 + strlen(_search_str));
              if (!Jdb_input::get_string(_search_str, sizeof(_search_str)) ||
                  !_search_str[0])
                goto status_line;
              // fall through
            case 'n': // search next
            case 'N': // search next reverse
              n = search(_absy + addy, entries, _search_str,
                         c == 'N' ? !direction : direction);
              if (n != Nil)
                {
                  // found
 jump_index:
                  if (n < _absy || n > _absy + lines - 1)
                    {
                      // screen crossed
                      addy = 4;
                      _absy = n - addy;
                      if (n < addy)
                        {
                          addy = n;
                          _absy = 0;
                        }
                      if (_absy > max_absy)
                        {
                          _absy = max_absy;
                          addy = n - _absy;
                        }
                      redraw = true;
                      break;
                    }
                  else
                    addy = n - _absy;
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
  _nr_cur = (e = Jdb_tbuf::lookup(_absy+addy)) ? e->number() : 0;
}

PUBLIC
Jdb_module::Action_code
Jdb_tbuf_show::action(int cmd, void *&, char const *&, int &)
{
  switch (cmd)
    {
    case 0:
      show();
      break;

    case 1:
      Jdb_tbuf_output::set_filter(_filter_str, 0);
      show_events(0, 0, 1000000, 0, 0, 1);
      break;

    case 2:
      if (Kconsole::console()->find_console(Console::GZIP))
        {
          Jdb_tbuf_output::set_filter(_filter_str, 0);
          Kconsole::console()->start_exclusive(Console::GZIP);
          show_events(0, 0, 1000000, 0, 0, 1);
          Kconsole::console()->end_exclusive(Console::GZIP);
        }
      break;
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_tbuf_show::cmds() const
{
  static Cmd cs[] =
    {
        { 0, "T", "tbuf", "",
          "T{P{+|-|k|u|<event>}}\tenter tracebuffer, on/off/kernel/user perf",
          0 },
        { 1, "", "tbufdumptext", "", 0 /* invisible */, 0 },
        { 2, "", "tbufdumpgzip", "", 0 /* invisible */, 0 },
    };

  return cs;
}

PUBLIC
int
Jdb_tbuf_show::num_cmds() const
{
  return 3;
}

IMPLEMENT
Jdb_tbuf_show::Jdb_tbuf_show()
    : Jdb_module("MONITORING")
{}

static Jdb_tbuf_show jdb_tbuf_show INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
