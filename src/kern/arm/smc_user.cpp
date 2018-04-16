IMPLEMENTATION[arm_smc_user]:

#include "kobject_helper.h"
#include "smc_call.h"

JDB_DEFINE_TYPENAME(Smc_user, "SMC");

struct Smc_user : Kobject_h<Smc_user, Kobject>
{
  L4_msg_tag kinvoke(L4_obj_ref, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb const *in, Utcb *out)
  {
    L4_msg_tag tag = f->tag();

    if (!Ko::check_basics(&tag, rights, L4_msg_tag::Label_smc))
      return tag;

    if (f->tag().words() != 8)
      return commit_result(-L4_err::EInval);

    // only allow calls to trusted applications or a trusted OS
    if ((in->values[0] & 0x3F000000) < 0x30000000)
      return commit_result(-L4_err::ENosys);

    register Mword r0 FIASCO_ARM_ASM_REG(0) = in->values[0];
    register Mword r1 FIASCO_ARM_ASM_REG(1) = in->values[1];
    register Mword r2 FIASCO_ARM_ASM_REG(2) = in->values[2];
    register Mword r3 FIASCO_ARM_ASM_REG(3) = in->values[3];
    register Mword r4 FIASCO_ARM_ASM_REG(4) = in->values[4];
    register Mword r5 FIASCO_ARM_ASM_REG(5) = in->values[5];
    register Mword r6 FIASCO_ARM_ASM_REG(6) = in->values[6];
    register Mword r7 FIASCO_ARM_ASM_REG(7) = in->values[7];

    asm volatile("smc #0" FIASCO_ARM_SMC_CALL_ASM_OPERANDS);

    out->values[0] = r0;
    out->values[1] = r1;
    out->values[2] = r2;
    out->values[3] = r3;

    return commit_result(0, 4);


  }
};

static Static_object<Smc_user> _glbl_smc_user;

PUBLIC static
void
Smc_user::init()
{
  _glbl_smc_user.construct();
  initial_kobjects.register_obj(_glbl_smc_user, Initial_kobjects::Smc);
}

STATIC_INITIALIZE(Smc_user);
