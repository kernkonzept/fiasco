INTERFACE [riscv]:

#include "types.h"

/**
 * The Supervisor Binary Interface (SBI) is a standardized abstraction layer
 * for platform-specific functionality, which is provided by the supervisor
 * execution environment (SEE), so either a machine mode firmware or
 * a hypervisor. Decoupling supervisor software, such as Fiasco, from the
 * implementation details of any particular SEE eases porting.
 *
 * The SBI function call mechanism is designed around a small core, with
 * additional functionality provided through a set of optional extensions.
 *
 * Starting with SBI v0.2 it is possible to probe if a specific extension is
 * available. An extension is identified by a unique 32-bit word,
 * the extension ID. An Extension consists of functions, which are identified
 * by the extension ID and a function ID.
 *
 * The Sbi class contains bindings for a set of important extensions or at
 * least the functions of them that are relevant for Fiasco.
 *
 * For platforms that provide only the old legacy SBI v0.1, there is the
 * `riscv_sbi_v1` Kconfig option, which transparently switches the
 * implementation of some of the basic functionality (set_timer, send_ipi and
 * remote_fence_i) to use the legacy SBI v0.1. Extensions cannot be used in
 * this configuration, as indicated by Sbi::Ext::probe(), which will always
 * return false.
 */
class Sbi
{
public:
  static void init();

  static void set_timer(Unsigned64 stime_value);
  static void console_putchar(int ch);
  static int console_getchar();
  static void send_ipi(Mword hart_mask, Mword hart_mask_base = 0);
  static void remote_fence_i(Mword hart_mask);
  static FIASCO_NORETURN void shutdown();

private:
  struct Ret
  {
    long error;
    long value;
  };

  enum : Signed32
  {
    Ext_base    = 0x10,
    Ext_time    = 0x54494D45,
    Ext_ipi     = 0x735049,
    Ext_rfnc    = 0x52464E43,
    Ext_hsm     = 0x48534D,
    Ext_srst    = 0x53525354,
  };

  template<Signed32 Ext_id>
  class Ext
  {
  public:
    static bool probe();

  protected:
    static Ret sbi_call(Mword func_id,
                        Mword arg0 = 0, Mword arg1 = 0, Mword arg2 = 0,
                        Mword arg3 = 0, Mword arg4 = 0, Mword arg5 = 0);
  };

public:
  class Base : public Ext<Ext_base>
  {
  public:
    static long get_spec_version();
    static long get_impl_id();
    static long get_impl_version();
    static bool probe_extension(Signed32 ext_id);

  private:
    enum
    {
      Func_get_sbi_spec_version = 0,
      Func_get_sbi_impl_id      = 1,
      Func_get_sbi_impl_version = 2,
      Func_probe_extension      = 3,
      Func_get_mvendorid        = 4,
      Func_get_marchid          = 5,
      Func_get_mimpid           = 6,
    };
  };

  class Time : public Ext<Ext_time>
  {
  public:
    static void set_timer(Unsigned64 stime_value);

  private:
    enum
    {
      Func_set_timer = 0,
    };
  };

  class Ipi : public Ext<Ext_ipi>
  {
  public:
    static void send_ipi(Mword hart_mask, Mword hart_mask_base = 0);

  private:
    enum
    {
      Func_send_ipi = 0,
    };
  };

  class Rfnc : public Ext<Ext_rfnc>
  {
  public:
    static void remote_fence_i(Mword hart_mask, Mword hart_mask_base);

  private:
    enum
    {
      Func_remote_fence_i  = 0,
    };
  };

  class Hsm : public Ext<Ext_hsm>
  {
  public:
    static bool hart_start(Mword hartid, Mword start_addr, Mword priv);
    static FIASCO_NORETURN void hart_stop();

    enum Hart_status
    {
      Hart_invalid               = -1,
      Hart_started               = 0,
      Hart_stopped               = 1,
      Hart_start_request_pending = 2,
      Hart_stop_request_pending  = 3,
    };
    static Hart_status hart_status(Mword hartid);

  private:
    enum
    {
      Func_hart_start      = 0,
      Func_hart_stop       = 1,
      Func_hart_get_status = 2,
    };
  };

  class Srst : public Ext<Ext_srst>
  {
  public:
    enum Reset_type : Mword
    {
      Type_shutdown    = 0,
      Type_cold_reboot = 1,
      Type_warm_reboot = 2,
    };
    enum Reset_reason : Mword
    {
      Reason_no_reason      = 0,
      Reason_system_failure = 1,
    };
    static bool system_reset(Reset_type reset_type, Reset_reason reset_reason);

  private:
    enum
    {
      Func_system_reset = 0,
    };
  };

  class Legacy
  {
  public:
    static void set_timer(Unsigned64 stime_value);
    static void console_putchar(int ch);
    static int console_getchar();
    static void send_ipi(Mword hart_mask);
    static void remote_fence_i(Mword hart_mask);
    static FIASCO_NORETURN void shutdown();

  private:
    enum
    {
      Ext_0_1_set_timer              = 0,
      Ext_0_1_console_putchar        = 1,
      Ext_0_1_console_getchar        = 2,
      Ext_0_1_clear_ipi              = 3,
      Ext_0_1_send_ipi               = 4,
      Ext_0_1_remote_fence_i         = 5,
      Ext_0_1_remote_sfence_vma      = 6,
      Ext_0_1_remote_sfence_vma_asid = 7,
      Ext_0_1_shutdown               = 8,
    };

    static Mword sbi_call(Mword ext_id, Mword arg0 = 0, Mword arg1 = 0);
  };

private:
  enum
  {
    Sbi_success               =  0,
    Sbi_err_failed            = -1,
    Sbi_err_not_supported     = -2,
    Sbi_err_invalid_param     = -3,
    Sbi_err_denied            = -4,
    Sbi_err_invalid_address   = -5,
    Sbi_err_already_available = -6,
  };

  static Ret _sbi_call(Mword ext_id, Mword func_id, Mword arg0, Mword arg1,
                       Mword arg2, Mword arg3, Mword arg4, Mword arg5);

  static Signed32 spec_version;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "panic.h"
#include "warn.h"

Signed32 Sbi::spec_version = 0x1; // SBI specification v0.1

PUBLIC static inline
Signed32
Sbi::spec_major_version()
{
  return (spec_version >> 24) & 0x7f;
}

PUBLIC static inline
Signed32
Sbi::spec_minor_version()
{
  return spec_version & 0xffffff;
}

IMPLEMENT static inline
Mword
Sbi::Legacy::sbi_call(Mword ext_id, Mword arg0, Mword arg1)
{
  register Mword a0 asm("a0") = arg0;
  register Mword a1 asm("a1") = arg1;
  register Mword a7 asm("a7") = ext_id;
  __asm__ __volatile__ ("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

IMPLEMENT inline
void
Sbi::Legacy::set_timer(Unsigned64 stime_value)
{
  #if __riscv_xlen == 32
    sbi_call(Ext_0_1_set_timer, stime_value, stime_value >> 32);
  #else
    sbi_call(Ext_0_1_set_timer, stime_value);
  #endif
}

IMPLEMENT inline
void
Sbi::Legacy::console_putchar(int ch)
{
  sbi_call(Ext_0_1_console_putchar, ch);
}

IMPLEMENT inline
int
Sbi::Legacy::console_getchar()
{
  return sbi_call(Ext_0_1_console_getchar);
}

IMPLEMENT inline
void
Sbi::Legacy::send_ipi(Mword hart_mask)
{
  sbi_call(Ext_0_1_send_ipi, reinterpret_cast<Mword>(&hart_mask));
}

IMPLEMENT inline
void
Sbi::Legacy::remote_fence_i(Mword hart_mask)
{
  sbi_call(Ext_0_1_remote_fence_i, reinterpret_cast<Mword>(&hart_mask));
}

IMPLEMENT inline
void
Sbi::Legacy::shutdown()
{
  sbi_call(Ext_0_1_shutdown);
  __builtin_unreachable();
}

IMPLEMENT inline
void
Sbi::console_putchar(int ch)
{ Legacy::console_putchar(ch); }

IMPLEMENT inline
int
Sbi::console_getchar()
{ return Legacy::console_getchar(); }


IMPLEMENT inline
void
Sbi::shutdown()
{
  if (Srst::probe())
  {
    Srst::system_reset(Srst::Type_shutdown,
                       Srst::Reason_no_reason);
    __builtin_unreachable();
  }
  else
    Legacy::shutdown();
}

// We have to move the inline assembly into a separate function,
// because gcc ignores assigned registers in template functions.
// Bug 33661 (fixed with gcc 8.5.0, 9.4.0, 10.3.0 and 11.1.0)
IMPLEMENT inline
Sbi::Ret
Sbi::_sbi_call(Mword ext_id, Mword func_id, Mword arg0, Mword arg1,
               Mword arg2, Mword arg3, Mword arg4, Mword arg5)
{
  register Mword a0 asm("a0") = arg0;
  register Mword a1 asm("a1") = arg1;
  register Mword a2 asm("a2") = arg2;
  register Mword a3 asm("a3") = arg3;
  register Mword a4 asm("a4") = arg4;
  register Mword a5 asm("a5") = arg5;
  register Mword a6 asm("a6") = func_id;
  register Mword a7 asm("a7") = ext_id;

  __asm__ __volatile__ ("ecall"
    : "+r"(a0), "+r"(a1)
    : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
    : "memory");

  Ret ret;
  ret.error = a0;
  ret.value = a1;

  return ret;
}

IMPLEMENT template<Signed32 Ext_id> inline
Sbi::Ret
Sbi::Ext<Ext_id>::sbi_call(Mword func_id,
                           Mword arg0 = 0, Mword arg1 = 0, Mword arg2 = 0,
                           Mword arg3 = 0, Mword arg4 = 0, Mword arg5 = 0)
{
  return _sbi_call(Ext_id, func_id, arg0, arg1, arg2, arg3, arg4, arg5);
}

IMPLEMENT
long
Sbi::Base::get_spec_version()
{
  return sbi_call(Func_get_sbi_spec_version).value;
}
IMPLEMENT
long
Sbi::Base::get_impl_id()
{
  return sbi_call(Func_get_sbi_impl_id).value;
}
IMPLEMENT
long
Sbi::Base::get_impl_version()
{
  return sbi_call(Func_get_sbi_impl_version).value;
}
IMPLEMENT inline
bool
Sbi::Base::probe_extension(Signed32 ext_id)
{
  return sbi_call(Func_probe_extension, ext_id).value != 0;
}

IMPLEMENT
void
Sbi::Time::set_timer(Unsigned64 stime_value)
{
  #if __riscv_xlen == 32
    sbi_call(Func_set_timer, stime_value, stime_value >> 32);
  #else
    sbi_call(Func_set_timer, stime_value);
  #endif
}

IMPLEMENT
void
Sbi::Ipi::send_ipi(Mword hart_mask, Mword hart_mask_base)
{
  auto ret = sbi_call(Func_send_ipi, hart_mask, hart_mask_base);
  if (ret.error != Sbi_success)
    WARN("send_ipi: failed (error [%ld])\n", ret.error);
}

IMPLEMENT
void
Sbi::Rfnc::remote_fence_i(Mword hart_mask, Mword hart_mask_base)
{
  auto ret = sbi_call(Func_remote_fence_i, hart_mask, hart_mask_base);
  if (ret.error != Sbi_success)
    WARN("remote_fence_i: failed (error [%ld])\n", ret.error);
}

IMPLEMENT
bool
Sbi::Hsm::hart_start(Mword hartid, Mword start_addr, Mword priv)
{
  switch (sbi_call(Func_hart_start, hartid, start_addr, priv).error)
    {
      case Sbi_success:
      case Sbi_err_already_available:
        return true;
      case Sbi_err_invalid_address:
        WARN("sbi_hart_start: Invalid start address 0x" L4_MWORD_FMT
             " for hart " L4_MWORD_FMT "\n", start_addr, hartid);
        return false;
      case Sbi_err_invalid_param:
        WARN("sbi_hart_start: Invalid hartid 0x" L4_MWORD_FMT "\n", hartid);
        return false;
      case Sbi_err_failed:
      default:
        WARN("sbi_hart_start: Failed to start hart 0x" L4_MWORD_FMT "\n",
             hartid);
        return false;
    }
}

IMPLEMENT
void
Sbi::Hsm::hart_stop()
{
  sbi_call(Func_hart_stop);
  panic("sbi_hart_stop: Not excpected to return!");
  __builtin_unreachable();
}

IMPLEMENT
Sbi::Hsm::Hart_status
Sbi::Hsm::hart_status(Mword hartid)
{
  auto ret = sbi_call(Func_hart_get_status, hartid);
  return ret.error == Sbi_success ?
    static_cast<Hart_status>(ret.value) : Hart_invalid;
}

IMPLEMENT inline
bool
Sbi::Srst::system_reset(Reset_type reset_type, Reset_reason reset_reason)
{
  auto ret = sbi_call(Func_system_reset, reset_type, reset_reason).error;
  return ret == Sbi_success;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !riscv_sbi_v1]:

#include <cassert>

IMPLEMENT template<Signed32 Ext_id> inline
bool
Sbi::Ext<Ext_id>::probe()
{
  return Base::probe_extension(Ext_id);
}

IMPLEMENT
void
Sbi::init()
{
  long ret = Base::get_spec_version();
  if (ret > 0)
    spec_version = ret;

  printf("SBI specification v%u.%u detected.\n",
         spec_major_version(), spec_minor_version());

  if (spec_major_version() == 0 && spec_minor_version() < 2)
    panic("At least v0.2 of the SBI specification is required!");

  printf("SBI implementation ID=0x%lx version=%ld\n",
         Base::get_impl_id(), Base::get_impl_version());

  if (!Time::probe())
    panic("SBI v0.2 TIME extension not available!");

  if (!Ipi::probe())
    panic("SBI v0.2 IPI extension not available!");

  if (!Rfnc::probe())
    panic("SBI v0.2 RFENCE extension not available!");
}

IMPLEMENT inline
void
Sbi::set_timer(Unsigned64 stime_value)
{ Time::set_timer(stime_value); }

IMPLEMENT inline NEEDS[<cassert>]
void
Sbi::send_ipi(Mword hart_mask, Mword hart_mask_base = 0)
{
  (void)hart_mask_base;
  assert(hart_mask_base == 0);
  Ipi::send_ipi(hart_mask);
}

IMPLEMENT inline
void
Sbi::remote_fence_i(Mword hart_mask)
{ Rfnc::remote_fence_i(hart_mask, 0); }

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && riscv_sbi_v1]:

#include <cassert>

IMPLEMENT template<Signed32 Ext_id> inline
bool
Sbi::Ext<Ext_id>::probe()
{ return false; }

IMPLEMENT inline
void
Sbi::init()
{}

IMPLEMENT inline
void
Sbi::set_timer(Unsigned64 stime_value)
{ Legacy::set_timer(stime_value); }

IMPLEMENT inline NEEDS[<cassert>]
void
Sbi::send_ipi(Mword hart_mask, Mword hart_mask_base = 0)
{
  assert(hart_mask_base == 0);
  Legacy::send_ipi(hart_mask);
}

IMPLEMENT inline
void
Sbi::remote_fence_i(Mword hart_mask)
{ Legacy::remote_fence_i(hart_mask, 0); }
