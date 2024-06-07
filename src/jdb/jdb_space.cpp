//-----------------------------------------------------------------------
INTERFACE:

#include "config.h"
#include "jdb_kobject.h"
#include "l4_types.h"
#include "initcalls.h"


class Jdb_space_image_info : public Jdb_kobject_extension
{
public:
  static char const *const static_type;
  char const *type() const override { return static_type; }

  Jdb_space_image_info(Mword base, char const *name, int size);
  ~Jdb_space_image_info() {}

  void operator delete (void *);

private:
  Mword _base;
  char _name[16];
};

//-----------------------------------------------------------------------
IMPLEMENTATION:

#include "task.h"
#include "static_init.h"
#include "kmem_slab.h"

static DEFINE_GLOBAL Global_data<Kmem_slab_t<Jdb_space_image_info>>
  _sii_allocator("Jdb_space_image_info");
char const *const Jdb_space_image_info::static_type = "Jdb_space_image_info";

IMPLEMENT
void
Jdb_space_image_info::operator delete (void *p)
{
  _sii_allocator->free(p);
}

IMPLEMENT
Jdb_space_image_info::Jdb_space_image_info(Mword base, char const *name, int size)
: _base(base)
{
  int i = 0;
  if (size > max_len())
    size = max_len();
  for (; name[i] && i < size; ++i)
    _name[i] = name[i];

  for (; i < max_len(); ++i)
    _name[i] = 0;
}

PUBLIC inline
int
Jdb_space_image_info::max_len()
{ return sizeof(_name); }

PUBLIC inline
Mword
Jdb_space_image_info::base() const
{ return _base; }

PUBLIC inline
char const *
Jdb_space_image_info::name() const
{ return _name; }


class Jdb_space : public Jdb_kobject_handler
{
public:
  Jdb_space() FIASCO_INIT;
};

IMPLEMENT
Jdb_space::Jdb_space()
  : Jdb_kobject_handler(static_cast<Task *>(nullptr))
{
  Jdb_kobject::module()->register_handler(this);
}

PUBLIC
bool
Jdb_space::invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb) override
{
  switch (utcb->values[0])
    {
    case Op_add_image_info:
      {
        if (f->tag().words() < 2)
          {
            f->tag(Kobject_iface::commit_result(-L4_err::EInval));
            return true;
          }

        Jdb_space_image_info *ne =
          _sii_allocator->new_obj(access_once(&utcb->values[1]),
                                  reinterpret_cast<char const *>(&utcb->values[2]),
                                  (f->tag().words() - 2) * sizeof(Mword));
        if (!ne)
          {
            f->tag(Kobject_iface::commit_result(-L4_err::ENomem));
            return true;
          }

        o->dbg_info()->_jdb_data.add(ne);
        f->tag(Kobject_iface::commit_result(0));
        return true;
      }
    }
  return false;
}

static DEFINE_GLOBAL_PRIO(JDB_MODULE_INIT_PRIO) Global_data<Jdb_space> hdl;

//-----------------------------------------------------------------------
IMPLEMENTATION [jdb]:

#include "kernel_task.h"
#include "keycodes.h"

class Jdb_space_mod : public Jdb_module
{
public:
  Jdb_space_mod() FIASCO_INIT;
private:
  static Task *task;
};

Task *Jdb_space_mod::task;

IMPLEMENT
Jdb_space_mod::Jdb_space_mod()
  : Jdb_module("INFO")
{}

PUBLIC
bool
Jdb_space::show_kobject(Kobject_common *o, int lvl) override
{
  Task *t = cxx::dyn_cast<Task*>(o);
  show(t);
  if (lvl)
    {
      Jdb::getchar();
      return true;
    }

  return false;
}

PUBLIC
void
Jdb_space::show_kobject_short(String_buffer *buf, Kobject_common *o, bool) override
{
  Task *t = cxx::dyn_cast<Task*>(o);
  if (t == Kernel_task::kernel_task())
    buf->printf(" {KERNEL}");

  buf->printf(" R=%ld", t->ref_cnt());
}

PUBLIC
bool
Jdb_space::info_kobject(Jobj_info *i, Kobject_common *o) override
{
  Task *t = cxx::dyn_cast<Task*>(o);

  i->type = i->space.Type;
  i->space.is_kernel = t == Kernel_task::kernel_task();
  i->space.ref_cnt = t->ref_cnt();
  return true;
}

PRIVATE
void
Jdb_space::show(Task *t)
{
  Jdb::cursor(3, 1);
  Jdb::line();
  printf("\nSpace %p (Kobject*)%p%s\n", static_cast<void *>(t),
         static_cast<void *>(static_cast<Kobject*>(t)),
         Jdb::clear_to_eol_str());

  for (auto const &m : t->_ku_mem)
    printf("  utcb area: user_va=%p kernel_va=%p size=%x%s\n",
           m->u_addr.get(), m->k_addr, m->size, Jdb::clear_to_eol_str());

  unsigned long m = t->ram_quota()->current();
  printf("  mem usage: %lu (%luKB) ", m, m/1024);
  if (t->ram_quota()->unlimited())
    printf("-- unlimited%s\n", Jdb::clear_to_eol_str());
  else
    {
      unsigned long l = t->ram_quota()->limit();
      printf("of %lu (%luKB) @%p%s\n", l, l/1024,
             static_cast<void *>(t->ram_quota()), Jdb::clear_to_eol_str());
    }

  for (int i = 0;
       auto *info = Jdb_kobject_extension::find_extension<Jdb_space_image_info>(t, i);
       i++)
    printf("  image: %.*s@0x%lx%s\n",
           info->max_len(), info->name(), info->base(), Jdb::clear_to_eol_str());

  Jdb::line();
}

static bool space_filter(Kobject_common const *o)
{ return cxx::dyn_cast<Task const *>(o); }

PUBLIC
Jdb_module::Action_code
Jdb_space_mod::action(int cmd, void *&, char const *&, int &) override
{
  if (cmd == 0)
    {
      Jdb_kobject_list list(space_filter);
      list.do_list();
    }
  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_space_mod::cmds() const override
{
  static Cmd cs[] =
    {
	{ 0, "s", "spacelist", "", "s\tshow task list", 0 },
    };
  return cs;
}
  
PUBLIC
int
Jdb_space_mod::num_cmds() const override
{ return 1; }

static
bool
filter_task_thread(Kobject_common const *o)
{
  return cxx::dyn_cast<Task const *>(o) || cxx::dyn_cast<Thread const *>(o);
}
static Jdb_space_mod jdb_space INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[Tasks + Threads]", filter_task_thread);

