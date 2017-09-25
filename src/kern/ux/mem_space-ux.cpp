/*
 * Fiasco-UX
 * Architecture specific pagetable code
 */

INTERFACE[ux]:

#include "entry_frame.h"
#include <sys/types.h>		// for pid_t
#include "kmem.h"
#include "mem_layout.h"
#include "paging.h"

extern "C" FIASCO_FASTCALL
int
thread_page_fault(Address, Mword, Address, Mword, Return_frame *);


EXTENSION class Mem_space
{
protected:
  int sync_kernel() const { return 0; }

  pid_t _pid;
};

IMPLEMENTATION[ux]:

#include <unistd.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include "boot_info.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "mem_layout.h"
#include "processor.h"
#include "regdefs.h"
#include "trampoline.h"
#include <cstring>
#include "config.h"
#include "emulation.h"
#include "logdefs.h"

PROTECTED static inline NEEDS ["kmem.h", "emulation.h"]
Pdir *
Mem_space::current_pdir()
{
  return reinterpret_cast<Pdir*>(Kmem::phys_to_virt(Emulation::pdir_addr()));
}

IMPLEMENT inline NEEDS ["kmem.h", "emulation.h"]
void
Mem_space::make_current()
{
  Emulation::set_pdir_addr(Kmem::virt_to_phys(_dir));
  _current.cpu(current_cpu()) = this;
}

// returns host pid number
PUBLIC inline
pid_t
Mem_space::pid() const
{ return _pid; }

// sets host pid number
PUBLIC inline
void
Mem_space::set_pid(pid_t pid)
{ _pid = pid; }

IMPLEMENT inline NEEDS["logdefs.h"]
void
Mem_space::switchin_context(Mem_space *from)
{
  if (this == from)
    return;

  CNT_ADDR_SPACE_SWITCH;
  make_current();
}

IMPLEMENT inline NEEDS [<asm/unistd.h>, <sys/mman.h>, "boot_info.h",
                        "cpu_lock.h", "lock_guard.h", "mem_layout.h",
                        "trampoline.h"]
void
Mem_space::page_map(Address phys, Address virt, Address size, Attr attr)
{
  auto guard = lock_guard(cpu_lock);

  Mword *trampoline = (Mword *)Mem_layout::kernel_trampoline_page;

  *(trampoline + 1) = virt;
  *(trampoline + 2) = size;
  *(trampoline + 3) = PROT_READ | (attr.rights & Page::Rights::W() ? PROT_WRITE : 0);
  *(trampoline + 4) = MAP_SHARED | MAP_FIXED;
  *(trampoline + 5) = Boot_info::fd();

  if (phys >= Boot_info::fb_virt() &&
      phys + size <= Boot_info::fb_virt() +
                     Boot_info::fb_size() +
                     Boot_info::input_size())
    *(trampoline + 6) = Boot_info::fb_phys() + (phys - Boot_info::fb_virt());
  else
    *(trampoline + 6) = phys;

  Trampoline::syscall(pid(), __NR_mmap, Mem_layout::Trampoline_page + 4);
}

IMPLEMENT inline NEEDS [<asm/unistd.h>, "cpu_lock.h", "lock_guard.h",
                        "trampoline.h"]
void
Mem_space::page_unmap(Address virt, Address size)
{
  auto guard = lock_guard(cpu_lock);

  Trampoline::syscall(pid(), __NR_munmap, virt, size);
}

IMPLEMENT inline NEEDS [<asm/unistd.h>, "cpu_lock.h", "lock_guard.h",
                        "trampoline.h"]
void
Mem_space::page_protect(Address virt, Address size, unsigned attr)
{
  auto guard = lock_guard(cpu_lock);

  Trampoline::syscall(pid(), __NR_mprotect, virt, size,
                      PROT_READ | (attr & Page_writable ? PROT_WRITE : 0));
}


/**
 * Translate virtual memory address in user space to virtual memory
 * address in kernel space (requires all physical memory be mapped).
 * @param addr Virtual address in user address space.
 * @param write Type of memory access (read or write) for page-faults.
 * @return Virtual address in kernel address space.
 */
PRIVATE inline NEEDS[Mem_space::current_pdir]
template< typename T >
T *
Mem_space::user_to_kernel(T const *addr, bool write)
{
  Phys_addr phys;
  Virt_addr virt = Virt_addr((Address) addr);
  Attr attr;
  unsigned error = 0;
  Page_order size;

  for (;;)
    {
      // See if there is a mapping for this address
      if (v_lookup(virt, &phys, &size, &attr))
        {
          // Add offset to frame
          phys = phys | cxx::get_lsb(virt, size);

          // See if we want to write and are not allowed to
          // Generic check because INTEL_PTE_WRITE == INTEL_PDE_WRITE
          if (!write || (attr.rights & Page::Rights::W()))
            return (T*)Mem_layout::phys_to_pmem(Phys_addr::val(phys));

          error |= PF_ERR_PRESENT;
        }

      if (write)
        error |= PF_ERR_WRITE;

      // If we tried to access user memory of a space other than current_mem_space()
      // our Long-IPC partner must do a page-in. This is analogue to IA32
      // page-faulting in the IPC window. Set PF_ERR_REMTADDR hint.
      // Otherwise we faulted on our own user memory. Set PF_ERR_USERADDR hint.
      error |= (dir() == current_pdir() ? PF_ERR_USERADDR : PF_ERR_REMTADDR);

      // No mapping or insufficient access rights, raise pagefault.
      // Pretend open interrupts, we restore the current state afterwards.
      Cpu_lock::Status was_locked = cpu_lock.test();

      thread_page_fault(Virt_addr::val(virt), error, 0, Proc::processor_state() | EFLAGS_IF, 0);

      cpu_lock.set (was_locked);
    }
}

IMPLEMENT inline NEEDS ["config.h", "cpu_lock.h", "lock_guard.h"]
template< typename T >
T
Mem_space::peek_user(T const *addr)
{
  T value;

  // Check if we cross page boundaries
  if (((Address)addr                   & Config::PAGE_MASK) ==
     (((Address)addr + sizeof (T) - 1) & Config::PAGE_MASK))
    {
      auto guard = lock_guard(cpu_lock);
      value = *user_to_kernel(addr, false);
    }
  else
    copy_from_user<T>(&value, addr, 1);

  return value;
}

IMPLEMENT inline NEEDS ["config.h", "cpu_lock.h", "lock_guard.h"]
template< typename T >
void
Mem_space::poke_user(T *addr, T value)
{
  // Check if we cross page boundaries
  if (((Address)addr                   & Config::PAGE_MASK) ==
     (((Address)addr + sizeof (T) - 1) & Config::PAGE_MASK))
    {
      auto guard = lock_guard(cpu_lock);
      *user_to_kernel(addr, true) = value;
    }
  else
    copy_to_user<T>(addr, &value, 1);
}

PRIVATE
template< typename T >
void
Mem_space::copy_from_user(T *kdst, T const *usrc, size_t n)
{
  auto guard = lock_guard(cpu_lock);

  char *ptr = (char *)usrc;
  char *dst = (char *)kdst;
  char *src = 0;

  n *= sizeof (T);

  while (n--)
    {
      if (!src || ((Address)ptr & ~Config::PAGE_MASK) == 0)
        src = user_to_kernel(ptr, false);

      *dst++ = *src++;
      ptr++;
    }
}

PRIVATE inline NEEDS ["config.h", "cpu_lock.h", "lock_guard.h"]
template< typename T >
void
Mem_space::copy_to_user(T *udst, T const *ksrc, size_t n)
{
  auto guard = lock_guard(cpu_lock);

  char *ptr = (char *)udst;
  char *src = (char *)ksrc;
  char *dst = 0;

  n *= sizeof (T);

  while (n--)
    {
      if (!dst || ((Address)ptr & ~Config::PAGE_MASK) == 0)
        dst = user_to_kernel(ptr, true);

      *dst++ = *src++;
      ptr++;
    }
}

