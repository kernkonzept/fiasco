IMPLEMENTATION:

#include <climits>
#include <cstring>
#include <cstdio>

#include "jdb.h"
#include "jdb_core.h"
#include "jdb_module.h"
#include "jdb_kobject.h"
#include "jdb_list.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "mem_unit.h"
#include "ram_quota.h"
#include "simpleio.h"
#include "task.h"
#include "static_init.h"


class Jdb_log_list : public Jdb_list
{
  friend class Jdb_log_list_hdl;
public:
  void *get_head() const { return _jdb_log_table; }
  char const *show_head() const { return "[Log]"; }

private:
  static Tb_log_table_entry *_end;
};

Tb_log_table_entry *Jdb_log_list::_end;

class Jdb_log_list_hdl : public Jdb_kobject_handler
{
public:
  virtual bool show_kobject(Kobject_common *, int) { return true; }
};

PUBLIC
bool
Jdb_log_list_hdl::invoke(Kobject_common *, Syscall_frame *f, Utcb *utcb)
{
  switch (utcb->values[0])
    {
      case Op_query_log_typeid:
          {
            unsigned char const idx = utcb->values[1];
            if (f->tag().words() < 3 || _jdb_log_table + idx >= &_jdb_log_table_end)
              {
                f->tag(Kobject_iface::commit_result(-L4_err::EInval));
                return true;
              }

            char nbuf[32];
            strncpy(nbuf, (char const *)&utcb->values[2], sizeof(nbuf));
            nbuf[sizeof(nbuf) - 1] = 0;

            Tb_log_table_entry *r;
            r = Jdb_log_list::find_next_log(nbuf, nbuf, _jdb_log_table + idx);

            utcb->values[0] = r ? (r - _jdb_log_table) + Tbuf_dynentries : ~0UL;
            f->tag(Kobject_iface::commit_result(0, 1));
            return true;
          }
      case Op_query_log_name:
          {
            unsigned char const idx = utcb->values[1];
            if (f->tag().words() != 2 || _jdb_log_table + idx >= &_jdb_log_table_end)
              {
                f->tag(Kobject_iface::commit_result(-L4_err::EInval));
                return true;
              }

            Tb_log_table_entry *e = _jdb_log_table + idx;
	    char *dst = (char *)&utcb->values[0];
            unsigned sz = strlen(e->name) + 1;
            sz += strlen(e->name + sz) + 1;
            if (sz > sizeof(utcb->values))
              sz = sizeof(utcb->values);
            memcpy(dst, e->name, sz);
            dst[sz - 1] = 0;

            f->tag(Kobject_iface::commit_result(0, (sz + sizeof(Mword) - 1)
                                                   / sizeof(Mword)));
            return true;
          }
      case Op_switch_log:
          {
            if (f->tag().words() < 3)
              {
                f->tag(Kobject_iface::commit_result(-L4_err::EInval));
                return true;
              }

            bool on = utcb->values[1];
            char nbuf[32];
            strncpy(nbuf, (char const *)&utcb->values[2], sizeof(nbuf));
            nbuf[sizeof(nbuf) - 1] = 0;

            Tb_log_table_entry *r = _jdb_log_table;
            while ((r = Jdb_log_list::find_next_log(nbuf, nbuf, r)))
              {
                Jdb_log_list::patch_item(r, on ? Jdb_log_list::patch_val(r) : 0);
                r++;
              }

            f->tag(Kobject_iface::commit_result(0));
            return true;
          }
    }

  return false;
}

PUBLIC
void
Jdb_log_list::show_item(String_buffer *buffer, void *item) const
{
  Tb_log_table_entry const *e = static_cast<Tb_log_table_entry const*>(item);
  char const *sc = e->name;
  sc += strlen(e->name) + 1;
  buffer->printf("%s %s (%s)",
                 Jdb_tbuf::get_entry_status(e) ? "[on ]" : "[off]",
                 e->name, sc);
}

PRIVATE static inline
unsigned
Jdb_log_list::patch_val(Tb_log_table_entry const *e)
{ return (e - _jdb_log_table) + Tbuf_dynentries; }

PRIVATE static
Tb_log_table_entry *
Jdb_log_list::find_next_log(const char *name, const char *sc,
                            Tb_log_table_entry *i)
{
  for (; i < _end; ++i)
    if (   !strcmp(name, i->name)
        || !strcmp(sc, i->name + strlen(i->name) + 1))
      return i;
  return 0;
}

PUBLIC
bool
Jdb_log_list::enter_item(void *item) const
{
  Tb_log_table_entry const *e = static_cast<Tb_log_table_entry const*>(item);
  patch_item(e, Jdb_tbuf::get_entry_status(e) ? 0 : patch_val(e));
  return true;
}

PRIVATE static
void
Jdb_log_list::patch_item(Tb_log_table_entry const *e, unsigned char val)
{
  if (e->patch)
    Jdb_tbuf::set_entry_status(e, val);

  for (Tb_log_table_entry *x = _end; x < &_jdb_log_table_end; ++x)
    {
      if (equal(x, e) && x->patch)
        Jdb_tbuf::set_entry_status(x, val);
    }
}

PRIVATE static
bool
Jdb_log_list::equal(Tb_log_table_entry const *a, Tb_log_table_entry const *b)
{
  if (strcmp(a->name, b->name))
    return false;

  char const *sca = a->name; sca += strlen(sca) + 1;
  char const *scb = b->name; scb += strlen(scb) + 1;

  if (strcmp(sca, scb))
    return false;

  return a->fmt == b->fmt;
}

PRIVATE
bool
Jdb_log_list::next(void **item)
{
  Tb_log_table_entry *e = static_cast<Tb_log_table_entry*>(*item);

  while (e + 1 < &_jdb_log_table_end)
    {
#if 0
      if (equal(e, e+1))
	++e;
      else
#endif
	{
	  *item  = e+1;
	  return true;
	}
    }

  return false;
}

PRIVATE
bool
Jdb_log_list::pref(void **item)
{
  Tb_log_table_entry *e = static_cast<Tb_log_table_entry*>(*item);

  if (e > _jdb_log_table)
    --e;
  else
    return false;
#if 0
  while (e > _jdb_log_table)
    {
      if (equal(e, e-1))
	--e;
      else
	break;
    }
#endif

  *item  = e;
  return true;
}

PUBLIC
int
Jdb_log_list::seek(int cnt, void **item)
{
  Tb_log_table_entry *e = static_cast<Tb_log_table_entry*>(*item);
  if (cnt > 0)
    {
      if (e + cnt >= _end)
	cnt = _end - e - 1;
    }
  else if (cnt < 0)
    {
      if (e + cnt < _jdb_log_table)
	cnt = _jdb_log_table - e;
    }

  if (cnt)
    {
      *item = e + cnt;
      return cnt;
    }

  return 0;
}

class Jdb_log : public Jdb_module
{
public:
  Jdb_log() FIASCO_INIT;
private:
};


static void swap(Tb_log_table_entry *a, Tb_log_table_entry *b)
{
  Tb_log_table_entry x = *a;
  *a = *b;
  *b = x;
}

static bool lt_cmp(Tb_log_table_entry *a, Tb_log_table_entry *b)
{
  if (strcmp(a->name, b->name) < 0)
    return true;
  else
    return false;
}

static void sort_tb_log_table()
{
  for (Tb_log_table_entry *p = _jdb_log_table; p < &_jdb_log_table_end; ++p)
    {
      for (Tb_log_table_entry *x = &_jdb_log_table_end -1; x > p; --x)
	if (lt_cmp(x, x - 1))
	  swap(x - 1, x);
    }
}

PUBLIC
static
void
Jdb_log_list::move_dups()
{
  _end = &_jdb_log_table_end;
  Tb_log_table_entry *const tab_end = &_jdb_log_table_end;
  for (Tb_log_table_entry *p = _jdb_log_table + 1; p < _end;)
    {
      if (equal(p-1, p))
	{
	  --_end;
	  if (p < _end)
	    {
	      Tb_log_table_entry tmp = *p;
	      memmove(p, p + 1, sizeof(Tb_log_table_entry) * (tab_end - p - 1));
	      *(tab_end - 1) = tmp;
	    }
	  else
	    break;
	}
      else
	++p;
    }
}

#if 0
static void disable_all()
{
  for (Tb_log_table_entry *p = _jdb_log_table; p < _jdb_log_table_end; ++p)
    *(p->patch) = 0;
}
#endif


IMPLEMENT
Jdb_log::Jdb_log()
  : Jdb_module("MONITORING")
{
  //disable_all();
  sort_tb_log_table();
  Jdb_log_list::move_dups();

  static Jdb_log_list_hdl hdl;
  Jdb_kobject::module()->register_handler(&hdl);
}

PUBLIC
Jdb_module::Action_code
Jdb_log::action(int, void *&, char const *&, int &)
{
  if (_jdb_log_table >= &_jdb_log_table_end)
    return NOTHING;

  Jdb_log_list list;
  list.set_start(_jdb_log_table);
  list.do_list();

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_log::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "O", "log", "", "O\tselect log events", 0 },
    };
  return cs;
}

PUBLIC
int
Jdb_log::num_cmds() const
{ return 1; }

static Jdb_log jdb_log INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

