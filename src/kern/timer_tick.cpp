INTERFACE:

#include "irq_chip.h"
#include "thread.h"

class Timer_tick : public Irq_base
{
public:
  enum Mode
  {
    Any_cpu, ///< Might hit on any CPU
    Sys_cpu, ///< Hit only on the CPU that manages the system time
    App_cpu, ///< Hit only on application CPUs
  };

  /// Create a timer IRQ object
  explicit Timer_tick(Mode mode);

  static void setup(Cpu_number cpu);
  static void enable(Cpu_number cpu);
  static void disable(Cpu_number cpu);

  static Timer_tick *boot_cpu_timer_tick();

protected:
  static bool attach_irq(Irq_base *irq, unsigned gsi);

private:
  // we do not support triggering modes
  void switch_mode(bool) override {}
};

// ------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Timer_tick
{
public:
  struct Log : public Tb_entry
  {
    Address user_ip;
    void print(String_buffer *) const;
  };
  static_assert(sizeof(Log) <= Tb_entry::Tb_entry_size);
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "timer.h"

#include "kdb_ke.h"
#include "kernel_console.h"

IMPLEMENT
Timer_tick::Timer_tick(Mode mode)
{
  switch (mode)
    {
    case Any_cpu: set_hit(&handler_all); break;
    case Sys_cpu: set_hit(&handler_sys_time); break;
    case App_cpu: set_hit(&handler_app); break;
    }
}

PRIVATE static inline NEEDS["thread.h", "timer.h", "kdb_ke.h",
                            "kernel_console.h", Timer_tick::serial_esc]
void
Timer_tick::handle_timer(Thread *t, Cpu_number cpu)
{
  Timer::update_system_clock(cpu);
  serial_esc(cpu);
  log_timer();
  t->handle_timer_interrupt();
}

PUBLIC static inline NEEDS[Timer_tick::handle_timer]
void
Timer_tick::handler_all_noack()
{ handle_timer(current_thread(), current_cpu()); }

PUBLIC static inline NEEDS[Timer_tick::handler_all_noack]
void
Timer_tick::handler_all(Irq_base *irq, Upstream_irq const *ui)
{
  nonull_static_cast<Timer_tick *>(irq)->ack();
  Upstream_irq::ack(ui);
  handler_all_noack();
}

PUBLIC static inline NEEDS[Timer_tick::handle_timer]
void
Timer_tick::handler_sys_time(Irq_base *irq, Upstream_irq const *ui)
{
  nonull_static_cast<Timer_tick *>(irq)->ack();
  Upstream_irq::ack(ui);
  // assume the boot CPU to be the CPU that manages the system time
  handle_timer(current_thread(), Cpu_number::boot_cpu());
}

PUBLIC static inline NEEDS["thread.h", "timer.h"]
void
Timer_tick::handler_app(Irq_base *_s, Upstream_irq const *ui)
{
  Timer_tick *self = nonull_static_cast<Timer_tick *>(_s);
  self->ack();
  Upstream_irq::ack(ui);
  log_timer();
  current_thread()->handle_timer_interrupt();
}

// --------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

PUBLIC static inline
void
Timer_tick::log_timer()
{}

// --------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "logdefs.h"
#include "irq_chip.h"
#include "string_buffer.h"

IMPLEMENT
void
Timer_tick::Log::print(String_buffer *buf) const
{
  buf->printf("u-ip=0x%lx", user_ip);
}

PUBLIC static inline NEEDS["logdefs.h"]
void
Timer_tick::log_timer()
{
  Context *c = current();
  LOG_TRACE("Timer IRQs (kernel scheduling)", "timer", c, Log,
      l->user_ip  = c->regs()->ip();
  );
}

// --------------------------------------------------------------------------
IMPLEMENTATION [!input]:

PRIVATE static inline void Timer_tick::enable_vkey(Cpu_number) {}
PRIVATE static inline void Timer_tick::serial_esc(Cpu_number) {}

// --------------------------------------------------------------------------
IMPLEMENTATION [input]:

#include "vkey.h"

/**
 * Tell Vkey that Timer_tick::handle_timer() calls Vkey::check() on input.
 * See handle_timer(). Call this from Timer_tick::setup() when running on the
 * boot CPU.
 */
PRIVATE static inline NEEDS["vkey.h"]
void
Timer_tick::enable_vkey(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    if (Config::esc_hack || Config::serial_input == Config::Serial_input_noirq)
      Vkey::enable_receive();
}

PRIVATE static inline
void
Timer_tick::serial_esc(Cpu_number cpu)
{
  if (   cpu == Cpu_number::boot_cpu()
      && (Config::esc_hack || Config::serial_input == Config::Serial_input_noirq))
    {
      if (Kconsole::console()->char_avail() > 0 && !Vkey::check_())
        kdb_ke("SERIAL_ESC");
    }
}
