IMPLEMENTATION [arm && !cpu_virt]:

PRIVATE inline
bool
Task::invoke_arch(L4_msg_tag &, Utcb *)
{
  return false;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && vgic]:

#include "vgic_global.h"

PRIVATE
L4_msg_tag
Task::map_gicc_page(L4_msg_tag tag, Utcb *utcb)
{
  if (tag.words() < 2)
    return commit_result(-L4_err::EInval);

  auto addr = Gic_h_global::gic->gic_v_address();
  if (!addr)
    return commit_result(-L4_err::ENosys);

  L4_fpage gicc_page(utcb->values[1]);
  if (   !gicc_page.is_valid()
      || !gicc_page.is_mempage()
      || gicc_page.order() < Config::PAGE_SHIFT)
    return commit_result(-L4_err::EInval);

  User<void>::Ptr u_addr((void *)gicc_page.mem_address());

  Mem_space *ms = static_cast<Mem_space *>(this);
  Mem_space::Status res =
    ms->v_insert(Mem_space::Phys_addr(addr),
                 Virt_addr((Address)u_addr.get()),
                 Mem_space::Page_order(Config::PAGE_SHIFT),
                 Mem_space::Attr(L4_fpage::Rights::URW()));

  switch (res)
    {
      case Mem_space::Insert_ok:
           break;
      case Mem_space::Insert_err_exists:
           return commit_result(-L4_err::EExists);
      case Mem_space::Insert_err_nomem:
           // FALLTHRU
      default:
           return commit_result(-L4_err::ENomem);
    };

  return commit_result(0);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && !vgic]:

PRIVATE
L4_msg_tag
Task::map_gicc_page(L4_msg_tag, Utcb *)
{
  return commit_result(-L4_err::ENosys);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

PRIVATE inline
bool
Task::invoke_arch(L4_msg_tag &tag, Utcb *utcb)
{
  if (utcb->values[0] == Vgicc_map_arm)
    {
      tag = map_gicc_page(tag, utcb);
      return true;
    }

  return false;
}

namespace {

static inline void
init_hyp_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             &Task::generic_factory<Task, false>);
}

STATIC_INITIALIZER(init_hyp_factory);

} // anon namespace
