#pragma once

#include "assert.h"
#include "cpu_lock.h"
#include "kip.h"
#include "processor.h"

class Poll_timeout_kclock
{
public:
  Poll_timeout_kclock(unsigned poll_time_us)
  {
    set(poll_time_us);
  }

  void set(unsigned poll_time_us)
  {
    _timeout = Kip::k()->clock + poll_time_us;
    _last_check = true;
  }

  bool test(bool expression = true)
  {
    // need open interrupts, otherwise clock won't tick
    assert(!cpu_lock.test());

    if (!expression)
      return false;

    Proc::irq_chance();
    return _last_check = Kip::k()->clock < _timeout;
  }

  bool timed_out() const { return !_last_check; }

private:
  Cpu_time _timeout;
  bool _last_check;
};
