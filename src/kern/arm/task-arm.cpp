IMPLEMENTATION [arm]:

PRIVATE inline
bool
Task::invoke_arch(L4_msg_tag &, Utcb *)
{
  return false;
}

IMPLEMENTATION [arm && cpu_virt]:

namespace {

static inline void
init_hyp_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             &Task::generic_factory<Task, false>);
}

STATIC_INITIALIZER(init_hyp_factory);

} // anon namespace

