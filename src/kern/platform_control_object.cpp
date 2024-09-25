INTERFACE:

#include "kobject_helper.h"
#include "platform_control.h"
#include "irq_controller.h"
#include "reset.h"
#include "global_data.h"

class Platform_control_object : public Kobject_h<Platform_control_object, Icu>
{
public:
  enum class Op : Mword
  {
    Suspend_system     = 0x0,
    Shutdown_system    = 0x1,
    Allow_cpu_shutdown = 0x2,
    Set_task_asid_arm  = 0x10,
  };

private:
  static Global_data<Platform_control_object> pfc;

protected:
  L4_msg_tag invoke_arch(L4_fpage::Rights rights, Syscall_frame *f,
                         Utcb const *r_msg, Utcb *s_msg);
};

//-----------------------------------------------------------------------------
IMPLEMENTATION:

JDB_DEFINE_TYPENAME(Platform_control_object, "Icu/Pfc");

DEFINE_GLOBAL Global_data<Platform_control_object> Platform_control_object::pfc;

PRIVATE inline
L4_msg_tag
Platform_control_object::sys_cpu_allow_shutdown(L4_fpage::Rights,
                                                Syscall_frame *f,
                                                Utcb const *utcb)
{
  // silently ignore it if we have no support for it
  if (!Platform_control::cpu_shutdown_available())
    return commit_result(0);

  if (f->tag().words() < 3)
    return commit_result(-L4_err::EInval);

  Cpu_number cpu = Cpu_number(utcb->values[1]);

  if (cpu >= Cpu::invalid() || !Per_cpu_data::valid(cpu))
    return commit_result(-L4_err::EInval);

  return commit_result(Platform_control::cpu_allow_shutdown(cpu, utcb->values[2]));
}

PRIVATE inline
L4_msg_tag
Platform_control_object::sys_system_suspend(L4_fpage::Rights, Syscall_frame *f,
                                            Utcb const *msg)
{
  Mword extra = 0;
  if (f->tag().words() >= 2)
    extra = msg->values[1];

  return commit_result(Platform_control::system_suspend(extra));
}

PRIVATE inline
L4_msg_tag
Platform_control_object::sys_system_shutdown(L4_fpage::Rights, Syscall_frame *f,
                                             Utcb const *msg)
{
  if (f->tag().words() != 2)
    return commit_result(-L4_err::EInval);

  if (msg->values[1] == 1) // reboot?
    Platform_control::system_reboot();

  if (msg->values[1] == 0) // shutdown
    Platform_control::system_off();

  // There's no generic in-kernel way for system power-off
  return commit_result(-L4_err::ENosys);
}

PUBLIC inline
L4_msg_tag
Platform_control_object::kinvoke(L4_obj_ref ref, L4_fpage::Rights rights,
                                 Syscall_frame *f, Utcb const *r_msg,
                                 Utcb *s_msg)
{
  L4_msg_tag tag = f->tag();

  if (tag.proto() == L4_msg_tag::Label_irq)
    return Icu::icu_invoke(ref, rights, f, r_msg, s_msg);

  if (!Ko::check_basics(&tag, 0))
    return tag;

  switch (static_cast<Op>(r_msg->values[0]))
    {
    case Op::Suspend_system:     return sys_system_suspend(rights, f, r_msg);
    case Op::Shutdown_system:    return sys_system_shutdown(rights, f, r_msg);
    case Op::Allow_cpu_shutdown: return sys_cpu_allow_shutdown(rights, f, r_msg);
    default:                     return invoke_arch(rights, f, r_msg, s_msg);
    }
}

IMPLEMENT_DEFAULT inline
L4_msg_tag
Platform_control_object::invoke_arch(L4_fpage::Rights, Syscall_frame *,
                                     Utcb const *, Utcb *)
{ return commit_result(-L4_err::ENosys); }
