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
  static bool allocate_irq(Irq_base *irq, unsigned irqnum);

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
#include "vkey.h"

IMPLEMENT
Timer_tick::Timer_tick(Mode mode)
{
  switch (mode)
    {
    case Any_cpu: set_hit(&handler_all); break;
    case Sys_cpu: set_hit(&handler_sys_time); break;
    case App_cpu: set_hit(&handler_app); break;
    }

  if (Config::esc_hack || (Config::serial_esc == Config::SERIAL_ESC_NOIRQ))
    Vkey::enable_receive();
}

PRIVATE static inline NEEDS["thread.h", "timer.h", "kdb_ke.h",
                            "kernel_console.h", "vkey.h"]
void
Timer_tick::handle_timer(Thread *t, Cpu_number cpu)
{
  Timer::update_system_clock(cpu);
  if (   (cpu == Cpu_number::boot_cpu())
      && (Config::esc_hack || (Config::serial_esc == Config::SERIAL_ESC_NOIRQ)))
    {
      if (Kconsole::console()->char_avail() > 0 && !Vkey::check_())
        kdb_ke("SERIAL_ESC");
    }
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
