#include <libc_backend.h>

#include <atomic.h>
#include <cpu_lock.h>
#include "mem.h"
#include "processor.h"

static Mword __libc_backend_printf_spinlock = ~0UL;

void __libc_backend_printf_local_force_unlock()
{
  Mword pid = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
  if (__libc_backend_printf_spinlock == pid)
    write_now(&__libc_backend_printf_spinlock, ~0UL);
}

unsigned long __libc_backend_printf_lock()
{
  unsigned long r = cpu_lock.test() ? 1 : 0;
  cpu_lock.lock();

  Mword pid = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());

  // support nesting
  if (__libc_backend_printf_spinlock == pid)
    return r | 2;

  for (;;)
    {
      Proc::pause();
      auto x = access_once(&__libc_backend_printf_spinlock);
      if (x != ~0UL)
        continue;

      if (mp_cas(&__libc_backend_printf_spinlock, (Mword)~0UL, pid))
        return r;
    }
}

void __libc_backend_printf_unlock(unsigned long state)
{
  if (!(state & 2))
    write_now(&__libc_backend_printf_spinlock, ~0UL);

  if (!(state & 1))
    cpu_lock.clear();

  Mem::mp_wmb();
}
