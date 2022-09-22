IMPLEMENTATION [explicit_asid]:

#include "task.h"

PRIVATE inline
L4_msg_tag
Platform_control_object::sys_set_asid(Syscall_frame *f, Utcb const *in)
{
  L4_msg_tag tag = f->tag();

  if (tag.words() != 2 || tag.items() != 1)
    return commit_result(-L4_err::EInval);

  L4_fpage::Rights rights = L4_fpage::Rights(0);
  auto *task = Ko::deref<Task>(&tag, in, &rights);
  if (!task)
    return tag;

  if (!(rights & L4_fpage::Rights::CW()))
    return Kobject_iface::commit_result(-L4_err::EPerm);

  task->asid(in->values[1]);

  return commit_result(0);
}

IMPLEMENT_OVERRIDE inline
L4_msg_tag
Platform_control_object::invoke_arch(L4_fpage::Rights, Syscall_frame *f,
                                     Utcb const *in, Utcb *)
{
  switch (static_cast<Op>(in->values[0]))
    {
    case Op::Set_task_asid_arm:
      return sys_set_asid(f, in);
    default:
      return commit_result(-L4_err::ENosys);
    }
}
