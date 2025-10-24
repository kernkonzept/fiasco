//-----------------------------------------------------------------------
INTERFACE:

#include "config.h"
#include "jdb_kobject.h"
#include "l4_types.h"
#include "initcalls.h"
#include "global_data.h"


class Jdb_kobject_name : public Jdb_kobject_extension
{
public:
  static char const *const static_type;
  virtual char const *type() const override { return static_type; }

  ~Jdb_kobject_name() {}

  void operator delete (void *);

private:
  char _name[16];
};

//-----------------------------------------------------------------------
IMPLEMENTATION:

#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "minmax.h"
#include "static_init.h"
#include "panic.h"

static DEFINE_GLOBAL Global_data<Kmem_slab_t<Jdb_kobject_name>>
  _name_allocator("Jdb_kobject_names");
char const *const Jdb_kobject_name::static_type = "Jdb_kobject_names";


PUBLIC
int
Jdb_kobject_name::max_len()
{ return sizeof(_name); }

PUBLIC
Jdb_kobject_name::Jdb_kobject_name()
{ _name[0] = 0; }

IMPLEMENT
void
Jdb_kobject_name::operator delete (void *p)
{
  _name_allocator->free(p);
}

PUBLIC
void
Jdb_kobject_name::name(char const *name, int size)
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
char const *
Jdb_kobject_name::name() const
{ return _name; }

PUBLIC inline
char *
Jdb_kobject_name::name()
{ return _name; }

class Jdb_name_hdl : public Jdb_kobject_handler
{
public:
  virtual ~Jdb_name_hdl() {}
};

PRIVATE static
void
Jdb_name_hdl::set_name(Kobject_common *o, Syscall_frame *f, Utcb *utcb)
{
  bool enqueue = false;
  auto *n = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o);
  if (!n)
    {
      n = _name_allocator->new_obj();
      if (!n)
        {
          f->tag(Kobject_iface::commit_result(-L4_err::ENomem));
          return;
        }
      enqueue = true;
    }

  if (f->tag().words() > 0)
    n->name(reinterpret_cast<char const *>(&utcb->values[1]),
            (f->tag().words() - 1) * sizeof(Mword));
  if (enqueue)
    o->dbg_info()->_jdb_data.add(n);

  f->tag(Kobject_iface::commit_result(0));
}

PRIVATE static
void
Jdb_name_hdl::get_name(Kobject_common *o, Syscall_frame *f, Utcb *utcb)
{
  if (auto *n = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o))
    {
      unsigned l = min<unsigned>(n->max_len(), sizeof(utcb->values) - 1);
      char *dst = reinterpret_cast<char *>(utcb->values);
      strncpy(dst, n->name(), l);
      dst[l] = 0;

      f->tag(Kobject_iface::commit_result(0, Ko::message_words(l + 1)));
    }
  else
    f->tag(Kobject_iface::commit_result(-L4_err::ENoent));
}

PUBLIC
bool
Jdb_name_hdl::invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb) override
{
  switch (Op{utcb->values[0]})
    {
    case Op::Set_name: // Set name for kernel object
      set_name(o, f, utcb);
      return true;

    case Op::Get_name: // Get name for kernel object
      get_name(o, f, utcb);
      return true;

    case Op::Query_name: // Query name for kernel object specified by debug ID
      query_name(f, utcb);
      return true;

    default:
      return false;
    }
}

//--------------------------------------------------------------------------
IMPLEMENTATION [!jdb]:

PRIVATE static inline
void
Jdb_name_hdl::query_name(Syscall_frame *f, Utcb *)
{
  f->tag(Kobject_iface::commit_result(-L4_err::ENosys));
}

//--------------------------------------------------------------------------
IMPLEMENTATION [jdb]:

PRIVATE static
void
Jdb_name_hdl::query_name(Syscall_frame *f, Utcb *utcb)
{
  if (auto d = Kobject_dbg::id_to_obj(utcb->values[1]); d != Kobject_dbg::end())
    get_name(Kobject::from_dbg(d), f, utcb);
  else
    f->tag(Kobject_iface::commit_result(-L4_err::ENoent));
}

PUBLIC
bool
Jdb_name_hdl::show_kobject(Kobject_common *, int) override
{ return true; }

PUBLIC
void
Jdb_name_hdl::show_kobject_short(String_buffer *buf, Kobject_common *o,
                                 bool) override
{
  if (auto *ex = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o))
    buf->printf(" {\033[1m%-.*s\033[m}", ex->max_len(), ex->name());
}

//--------------------------------------------------------------------------
IMPLEMENTATION:

DEFINE_GLOBAL static Global_data<Jdb_name_hdl> hdl;

PUBLIC static FIASCO_INIT
void
Jdb_kobject_name::init()
{
  Jdb_kobject::module()->register_handler(&hdl);
}

STATIC_INITIALIZE(Jdb_kobject_name);
