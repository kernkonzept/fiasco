IMPLEMENTATION [arm && mpu]:

#include "paging_bits.h"

IMPLEMENT_OVERRIDE inline void
Task::map_all_segs(Mem_desc::Mem_type mt)
{
  for (auto const &md: Kip::k()->mem_descs_a())
    {
      if (!md.valid() || md.is_virtual())
        continue;
      if (md.type() != mt)
        continue;
      if (!md.eager_map())
        continue;

      auto rights = L4_fpage::Rights::URWX();
      if (md.type() == Mem_desc::Bootloader)
        rights = L4_fpage::Rights::U() | L4_fpage::Rights(md.ext_type());
      auto attr = Mem_space::Attr::space_local(rights);
      for (auto addr = Pg::trunc(md.start());
           addr < md.end();
           addr += Config::PAGE_SIZE)
        Mem_space::v_insert(Mem_space::Phys_addr(addr), Virt_addr(addr),
                            Virt_order(Config::PAGE_SHIFT), attr);
    }
}

// ------------------------------------------------------------------------
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

  // Acquire reference to ensure the task is not deleted while we try to acquire
  // its existence lock.
  Ref_ptr self(this);

  // Acquire existence lock to prevent concurrent modification of the task's
  // page table.
  Lock_guard<Lock> guard_task;
  if (!guard_task.check_and_lock(&existence_lock))
    return commit_error(utcb, L4_error::Not_existent);

  User_ptr<void> u_addr(static_cast<void *>(gicc_page.mem_address()));

  Mem_space *ms = static_cast<Mem_space *>(this);
  Mem_space::Status res =
    ms->v_insert(Mem_space::Phys_addr(addr),
                 Virt_addr(u_addr.get()),
                 Mem_space::Page_order(Config::PAGE_SHIFT),
                 Mem_space::Attr(L4_fpage::Rights::URW(), Page::Type::Uncached(),
                                 Page::Kern::None(), Page::Flags::None()));

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

  _has_gicc_page_mapped = true;

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
  if (Op{utcb->values[0]} == Op::Vgicc_map_arm)
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
