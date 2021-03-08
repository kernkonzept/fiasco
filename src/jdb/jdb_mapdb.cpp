IMPLEMENTATION:

#include <cstdio>

#include "jdb.h"
#include "jdb_input.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "kobject.h"
#include "keycodes.h"
#include "mapdb.h"
#include "mapdb_i.h"
#include "map_util.h"
#include "simpleio.h"
#include "static_init.h"
#include "task.h"
#include "jdb_kobject.h"
#include "jdb_kobject_names.h"

class Jdb_mapdb : public Jdb_module
{
  friend class Jdb_kobject_mapdb_hdl;
public:
  Jdb_mapdb() FIASCO_INIT;
private:
  static Mword pagenum;
  static char  subcmd;
  static char  dump_tag[21];
};

Mword Jdb_mapdb::pagenum;
char  Jdb_mapdb::subcmd;
char  Jdb_mapdb::dump_tag[21];

static
const char*
size_str (Mword size)
{
  static char scratchbuf[6];
  unsigned mult = 0;
  while (size >= 1024)
    {
      size >>= 10;
      mult++;
    }
  snprintf(scratchbuf, 6, "%hu%c", (unsigned short)size, "BKMGTPX"[mult]);
  return scratchbuf;
}

static
unsigned long long
Jdb_mapdb::val(Mdb_types::Pfn p, Mdb_types::Order base_size)
{
  return cxx::int_value<Mdb_types::Pfn>(p << base_size);
}


static
bool
Jdb_mapdb::show_tree(Treemap* pages, Mapping::Pcnt offset, Mdb_types::Order base_size,
                     unsigned &screenline, unsigned indent = 1)
{
  typedef Treemap::Page Page;

  Page       page = pages->trunc_to_page(offset);
  Physframe*    f = pages->frame(page);
  Mapping_tree* t = f->tree();
  unsigned      i;
  int           c;

  if (!t || !*t->begin())
    {
      printf(" no mapping tree registered for frame number 0x%lx\033[K\n",
             cxx::int_value<Mapping::Page>(page));
      screenline++;
      return true;
    }

  printf(" mapping tree for %s-page %012llx of task %lx - header at "
         L4_PTR_FMT "\033[K\n",
         size_str(1ULL << cxx::int_value<Mdb_types::Order>(pages->_page_shift + base_size)),
         val(pages->vaddr(*t->begin()), base_size),
         Kobject_dbg::pointer_to_id(t->begin()->space()),
         (Address)t);

  printf(" header info: lock=%u\033[K\n", f->lock.test());

  auto m = t->begin();

  screenline += 2;

  unsigned empty = 0;
  unsigned c_depth = 0;
  for(i=0; *m; i++, ++m)
    {
      Kconsole::console()->getchar_chance();

      if (m->submap())
        printf("%*u: %lx  subtree@" L4_PTR_FMT,
               indent + c_depth > 10
                 ? 0 : (int)(indent + c_depth),
               i+1, (Address) *m, (Mword) m->submap());
      else
        {
          c_depth = m->depth();
          printf("%*u: %lx  va=%012llx  task=%lx  depth=",
                 indent + c_depth > 10 ? 0 : (int)(indent + c_depth),
                 i+1, (Address) *m,
                 val(pages->vaddr(*m), base_size),
                 Kobject_dbg::pointer_to_id(m->space()));

          if (m->is_root())
            printf("root");
          else if (m->unused())
            {
              printf("empty");
              ++empty;
            }
          else if (m->is_end_tag())
            printf("end");
          else
            printf("%u", c_depth);
        }

      puts("\033[K");
      screenline++;

      if (screenline >= (m->submap()
                         ? Jdb_screen::height() - 3
                         : Jdb_screen::height()))
        {
          printf(" any key for next page or <ESC>");
          Jdb::cursor(screenline, 33);
          c = Jdb_core::getchar();
          printf("\r\033[K");
          if (c == KEY_ESC)
            return false;
          screenline = 3;
          Jdb::cursor(3, 1);
        }

      if (m->submap())
        {
          if (! Jdb_mapdb::show_tree(m->submap(),
                                     cxx::get_lsb(offset, pages->_page_shift),
                                     base_size,
                                     screenline, indent + c_depth))
            return false;
        }
    }

  return true;
}

static
Address
Jdb_mapdb::end_address (Mapdb* mapdb)
{
  return cxx::int_value<Mdb_types::Pfn>(mapdb->dbg_treemap()->end_addr());
}

static
void
Jdb_mapdb::show(Mapping::Pfn page, char which_mapdb)
{
  unsigned     j;
  int          c;

  Jdb::clear_screen();
  typedef Mdb_types::Order Order;

  for (;;)
    {
      Mapdb* mapdb;
      const char* type;
      Mapping::Pcnt super_inc;
      Mdb_types::Order super_shift;
      Order base_size = Order(0);

      switch (which_mapdb)
        {
        case 'm':
          type = "Phys frame";
          mapdb = mapdb_mem.get();
          base_size = Order(Config::PAGE_SHIFT);
          super_shift = Mdb_types::Order(Config::SUPERPAGE_SHIFT - Config::PAGE_SHIFT);
          break;
#ifdef CONFIG_PF_PC
        case 'i':
          type = "I/O port";
          mapdb = mapdb_io.get();
          base_size = Order(0);
          super_shift = Mdb_types::Order(8);
          break;
#endif
        default:
          return;
        }

      super_inc = Mapping::Pcnt(1) << super_shift;

      if (! mapdb->valid_address(page))
        page = Mapping::Pfn(0);

      Jdb::cursor();
      printf("%s %012llx\033[K\n\033[K\n", type, val(page, base_size));

      j = 3;

      if (! Jdb_mapdb::show_tree(mapdb->dbg_treemap(), page - Mapping::Pfn(0), base_size, j))
        return;

      for (; j<Jdb_screen::height(); j++)
        puts("\033[K");

      static char prompt[] = "mapdb[m]";
      prompt[6] = which_mapdb;

      Jdb::printf_statline(prompt,
                           "n=next p=previous N=nextsuper P=prevsuper", "_");

      for (bool redraw=false; !redraw; )
        {
          Jdb::cursor(Jdb_screen::height(), 10);
          switch (c = Jdb_core::getchar())
            {
            case 'n':
            case KEY_CURSOR_DOWN:
              if (! mapdb->valid_address(++page))
                page = Mapping::Pfn(0);
              redraw = true;
              break;
            case 'p':
            case KEY_CURSOR_UP:
              if (! mapdb->valid_address(--page))
                page = Mapping::Pfn(end_address(mapdb) - 1);
              redraw = true;
              break;
            case 'N':
            case KEY_PAGE_DOWN:
              page = cxx::mask_lsb(page + super_inc, super_shift);
              if (! mapdb->valid_address(page))
                page = Mapping::Pfn(0);
              redraw = true;
              break;
            case 'P':
            case KEY_PAGE_UP:
              page = cxx::mask_lsb(page - super_inc, super_shift);
              if (! mapdb->valid_address(page))
                page = Mapping::Pfn(end_address(mapdb) - 1);
              redraw = true;
              break;
            case ' ':
              if (which_mapdb == 'm')
#ifdef CONFIG_PF_PC
                which_mapdb = 'i';
              else if (which_mapdb == 'i')
#endif
                which_mapdb = 'm';
              redraw = true;
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

PUBLIC
Jdb_module::Action_code
Jdb_mapdb::action(int cmd, void *&args, char const *&fmt, int &next_char) override
{
  static char which_mapdb = 'm';

  if (cmd == 1)
    {
      const char *tag = nullptr;
      if (args == (void*) dump_tag)
        tag = dump_tag;
      dump_all_obj_mappings(tag);
      return NOTHING;
    }

  if (cmd != 0)
    return NOTHING;

  if (args == (void*) &subcmd)
    {
      switch (subcmd)
        {
        default:
          return NOTHING;

        case '\r':
        case ' ':
          goto doit;

        case '0' ... '9':
        case 'a' ... 'f':
        case 'A' ... 'F':
          which_mapdb = 'm';
          fmt = " frame: " L4_FRAME_INPUT_FMT;
          args = &pagenum;
          next_char = subcmd;
          return EXTRA_INPUT_WITH_NEXTCHAR;

        case 'm':
          fmt = " frame: " L4_FRAME_INPUT_FMT;
          break;

#ifdef CONFIG_PF_PC
        case 'i':
          fmt = " port: " L4_FRAME_INPUT_FMT;
          break;
#endif
        }

      which_mapdb = subcmd;
      args = &pagenum;
      return EXTRA_INPUT;
    }

  else if (args != (void*) &pagenum)
    return NOTHING;

 doit:
  show(Mapping::Pfn(pagenum), which_mapdb);
  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_mapdb::cmds() const override
{
  static Cmd cs[] =
    {
        { 0, "m", "mapdb", "%C",
          "m[i]<pfn>\tshow [I/O] mapping database for PFN [port]",
          &subcmd },
        { 1, "", "dumpmapdbobjs", "%20s",
          "dumpmapdbobjs\tDump complete object mapping database", dump_tag },
    };
  return cs;
}

PUBLIC
int
Jdb_mapdb::num_cmds() const override
{
  return 2;
}

IMPLEMENT
Jdb_mapdb::Jdb_mapdb()
  : Jdb_module("INFO")
{}

static Jdb_mapdb jdb_mapdb INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

// --------------------------------------------------------------------------
// Handler for kobject list

class Jdb_kobject_mapdb_hdl : public Jdb_kobject_handler
{
public:
  bool show_kobject(Kobject_common *, int) override
  { return true; }

  virtual ~Jdb_kobject_mapdb_hdl() {}
};

PUBLIC static FIASCO_INIT
void
Jdb_kobject_mapdb_hdl::init()
{
  static Jdb_kobject_mapdb_hdl hdl;
  Jdb_kobject::module()->register_handler(&hdl);
}

PUBLIC
bool
Jdb_kobject_mapdb_hdl::handle_key(Kobject_common *o, int keycode) override
{
  if (keycode == 'm')
    {
      Jdb_mapdb::show_simple_tree(o);
      Jdb::getchar();
      return true;
    }
  else
    return false;
}

PUBLIC
char const *
Jdb_kobject_mapdb_hdl::help_text(Kobject_common *) const override
{
  return "m=mappings";
}


STATIC_INITIALIZE(Jdb_kobject_mapdb_hdl);

// --------------------------------------------------------------------------
IMPLEMENTATION:

#include "dbg_page_info.h"

static
void
Jdb_mapdb::print_obj_mapping(Obj::Mapping *m)
{
  Obj::Entry *e = static_cast<Obj::Entry*>(m);
  Dbg_page_info *pi = Dbg_page_info::table()[Virt_addr(e)];

  Mword space_id = ~0UL;
  Address cap_idx = ((Address)e % Config::PAGE_SIZE) / sizeof(Obj::Entry);

  String_buf<20> task_descr;
  if (pi)
    {
      Kobject_dbg *o =
        static_cast<Task*>(pi->info<Obj::Cap_page_dbg_info>()->s)->dbg_info();
      space_id = o->dbg_id();
      Jdb_kobject_name *n =
        Jdb_kobject_extension::find_extension<Jdb_kobject_name>(Kobject::from_dbg(o));
      if (n)
        task_descr.printf("(%.*s)", n->max_len(), n->name());
      cap_idx += pi->info<Obj::Cap_page_dbg_info>()->offset;
    }

  printf(L4_PTR_FMT "[C:%lx]: space=D:%lx%.*s rights=%x flags=%lx obj=%p",
         (Address)m, cap_idx, space_id, task_descr.length(), task_descr.begin(),
         (unsigned)cxx::int_value<Obj::Attr>(e->rights()),
         (unsigned long)e->_flags, e->obj());
}

static
bool
Jdb_mapdb::show_simple_tree(Kobject_common *f, unsigned indent = 1)
{
  (void)indent;
  (void)f;

  unsigned      screenline = 0;
  int           c;

  putchar('\r');
  Jdb::line();
  if (!f || f->map_root()->_root.empty())
    {
      printf(" no mapping tree registered for frame number 0x%lx\033[K\n",
             (unsigned long) f);
      screenline++;
      Jdb::line();
      return true;
    }

  printf(" mapping tree for object D:%lx (%p) ref_cnt=%ld\033[K\n",
         f->dbg_info()->dbg_id(), f, f->map_root()->_cnt);

  screenline += 2;

  for (auto const &&m: f->map_root()->_root)
    {
      Kconsole::console()->getchar_chance();

      printf("  ");
      print_obj_mapping(m);
      puts("\033[K");
      screenline++;

      if (screenline >= Jdb_screen::height())
        {
          printf(" any key for next page or <ESC>");
          Jdb::cursor(screenline, 33);
          c = Jdb_core::getchar();
          printf("\r\033[K");
          if (c == KEY_ESC)
            return false;
          screenline = 3;
          Jdb::cursor(3, 1);
        }
    }

  Jdb::line();
  return true;
}

static
void
Jdb_mapdb::dump_all_obj_mappings(char const *arg)
{
  if (!Kconsole::console()->find_console(Console::GZIP))
    return;

  int len = arg ? __builtin_strlen(arg) : 0;

  Kconsole::console()->start_exclusive(Console::GZIP);
  printf("========= OBJECT DUMP BEGIN ===================\n"
         "dump format version number: 0\n"
         "user space tag:%s%.*s\n", len ? " " : "", len, len ? arg : "");
  for (auto *d: Kobject_dbg::_kobjects)
    {
      Kobject *f = Kobject::from_dbg(d);
      String_buf<130> s;

      Jdb_kobject::obj_description(&s, NULL, true, d);
      printf("%.*s ref_cnt=%ld\n", s.length(), s.begin(), f->map_root()->_cnt);
      for (auto const &&m: f->map_root()->_root)
        {
          printf("\t");
          print_obj_mapping(m);
          printf("\n");
        }
    }
  printf("========= OBJECT DUMP END ===================\n");
  Kconsole::console()->end_exclusive(Console::GZIP);
}
