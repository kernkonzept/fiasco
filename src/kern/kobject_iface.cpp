INTERFACE:

#include "l4_types.h"
#include <cxx/dyn_cast>

class Kobject;
class Kobject_dbg;
class Kobject_mappable;

class Space;
class Ram_quota;
class Syscall_frame;
class Utcb;

class Kobject_common : public cxx::Dyn_castable<Kobject_common>
{
public:
  Kobject_common() = default;
  Kobject_common(Kobject_common const &) = delete;
  Kobject_common &operator = (Kobject_common const &) = delete;

  virtual bool is_local(Space *) const  = 0;
  virtual Mword obj_id() const  = 0;
  virtual void initiate_deletion(Kobject ***) = 0;

  virtual Kobject_mappable *map_root() = 0;
  virtual ~Kobject_common() = 0;
};

class Kobject_iface : public cxx::Dyn_castable<Kobject_iface, Kobject_common>
{
public:
  virtual void invoke(L4_obj_ref self, L4_fpage::Rights rights, Syscall_frame *, Utcb *) = 0;

  typedef Kobject_iface *Factory_func(Ram_quota *q,
                                      Space *current_space,
                                      L4_msg_tag tag,
                                      Utcb const *utcb, int *err);
  enum { Max_factory_index = -L4_msg_tag::Max_factory_label };
  static Factory_func *factory[Max_factory_index + 1];
};

IMPLEMENTATION:

#include "panic.h"

IMPLEMENT inline Kobject_common::~Kobject_common() {}

Kobject_iface::Factory_func *Kobject_iface::factory[Max_factory_index + 1];

PUBLIC static inline
L4_msg_tag
Kobject_iface::commit_result(Mword error,
                             unsigned words = 0, unsigned items = 0)
{
  return L4_msg_tag(words, items, 0, error);
}

PUBLIC static inline
L4_msg_tag
Kobject_iface::commit_error(Utcb const *utcb, L4_error const &e,
                            L4_msg_tag const &tag = L4_msg_tag(0, 0, 0, 0))
{
  const_cast<Utcb*>(utcb)->error = e;
  return L4_msg_tag(tag, L4_msg_tag::Error);
}

PUBLIC virtual
Kobject_iface *
Kobject_iface::downgrade(unsigned long del_attribs)
{ (void)del_attribs; return this; }

PUBLIC static
void
Kobject_iface::set_factory(long label, Factory_func *f)
{
  if (label > 0 || -label > Max_factory_index)
    panic("error: registering factory for invalid protocol/label: %ld\n",
          label);

  if (factory[-label])
    panic("error: factory for protocol/label %ld already registered: %p\n",
          label, factory[-label]);

  factory[-label] = f;
}

PUBLIC static inline
Kobject_iface *
Kobject_iface::manufacture(long label, Ram_quota *q,
                           Space *current_space,
                           L4_msg_tag tag, Utcb const *utcb, int *err)
{
  *err = L4_err::ENodev;
  if (EXPECT_FALSE(label > 0 || -label > Max_factory_index
                   || !factory[-label]))
    return 0;

  return factory[-label](q, current_space, tag, utcb, err);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC virtual Kobject_dbg *Kobject_common::dbg_info() const = 0;
