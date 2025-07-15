IMPLEMENTATION[arm_smc_user]:

#include "kobject_helper.h"
#include "kobject_rpc.h"
#include "smc_call.h"

JDB_DEFINE_TYPENAME(Smc_user, "SMC");

/*
 * L4-IFACE: kernel-smc.smc-call_arm
 * PROTOCOL: L4_PROTO_SMCCC
 */
struct Smc_user : Kobject_h<Smc_user, Kobject>
{
  L4_msg_tag kinvoke(L4_obj_ref, L4_fpage::Rights,
                     Syscall_frame *f, Utcb const *in, Utcb *out)
                     FIASCO_ARM_THUMB2_NO_FRAME_PTR
  {
    L4_msg_tag tag = f->tag();

    if (!Ko::check_basics(&tag, L4_msg_tag::Label_smc))
      return tag;

    if (f->tag().words() != 8)
      return commit_result(-L4_err::EInval);

    register Mword r0 FIASCO_ARM_ASM_REG(0) = access_once(&in->values[0]);

    // only allow fast calls
    if (!(r0 & 0x80000000))
      return commit_result(-L4_err::ENosys);

    // only allow calls in configured service call range
    if (   (r0 & 0x3F000000) < CONFIG_ARM_SMC_USER_MIN
        || (r0 & 0x3F000000) > CONFIG_ARM_SMC_USER_MAX)
      return commit_result(-L4_err::ENosys);

    if (!invoke_test_callback(in, out))
      return commit_result(0, 4);

    register Mword r1 FIASCO_ARM_ASM_REG(1) = in->values[1];
    register Mword r2 FIASCO_ARM_ASM_REG(2) = in->values[2];
    register Mword r3 FIASCO_ARM_ASM_REG(3) = in->values[3];
    register Mword r4 FIASCO_ARM_ASM_REG(4) = in->values[4];
    register Mword r5 FIASCO_ARM_ASM_REG(5) = in->values[5];
    register Mword r6 FIASCO_ARM_ASM_REG(6) = in->values[6];
    register Mword r7 FIASCO_ARM_ASM_REG(7) = in->values[7];

    asm volatile(FIASCO_ARM_ARCH_EXTENSION_SEC
                 "smc #0\n"
                 FIASCO_ARM_ARCH_EXTENSION_NOSEC
                 FIASCO_ARM_SMC_CALL_ASM_OPERANDS);

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
  initial_kobjects->register_obj(_glbl_smc_user, Initial_kobjects::Smc);
}

STATIC_INITIALIZE(Smc_user);

//---------------------------------------------------------------------------
INTERFACE [arm_smc_user && test_support_code]:

#include <cxx/function>

EXTENSION struct Smc_user
{
public:
  using Test_callback = cxx::functor<bool (Utcb const *in, Utcb *out)>;
  static Test_callback test_cb;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm_smc_user && test_support_code]:

Smc_user::Test_callback Smc_user::test_cb;

PUBLIC static inline
bool
Smc_user::invoke_test_callback(Utcb const *in, Utcb *out)
{
  if (test_cb)
    return test_cb(in, out);

  return true;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm_smc_user && !test_support_code]:

PUBLIC static inline
bool
Smc_user::invoke_test_callback(Utcb const *, Utcb *)
{
  return true;
}
