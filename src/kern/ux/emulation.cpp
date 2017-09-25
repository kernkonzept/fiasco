/*
 * Fiasco-UX
 * Emulation Code for some processor internals (registers, instructions)
 */

INTERFACE:

#include <types.h>
#include "mem_layout.h"

class Idt_entry;

struct Ldt_user_desc			// work around glibc naming mess
{
  unsigned int  entry_number;
  unsigned long base_addr;
  unsigned int  limit;
  unsigned int  seg_32bit:1;
  unsigned int  contents:2;
  unsigned int  read_exec_only:1;
  unsigned int  limit_in_pages:1;
  unsigned int  seg_not_present:1;
  unsigned int  useable:1;
};

class Pseudo_descriptor;

class Emulation
{
private:
  static Address	_page_dir_addr		asm ("PAGE_DIR_ADDR");
  static Address	_page_fault_addr	asm ("PAGE_FAULT_ADDR");
  static Idt_entry const *_idt_base;
  static unsigned short	_idt_limit;
  static unsigned       _host_tls_base;
};

IMPLEMENTATION:

#include <cassert>
#include <asm/unistd.h>
#include <sched.h>
#include "x86desc.h"

Address         Emulation::_page_dir_addr;
Address         Emulation::_page_fault_addr;
Idt_entry const *Emulation::_idt_base;
unsigned short	Emulation::_idt_limit;
unsigned        Emulation::_host_tls_base;

/**
 * Return page directory base address (register cr3)
 * @return Page Directory Base Address
 */
PUBLIC static inline
Address
Emulation::pdir_addr()
{
  return _page_dir_addr;
}

/**
 * Set page directory base address (register cr3)
 * @param addr New Page Directory Base Address
 */
PUBLIC static inline
void
Emulation::set_pdir_addr (Address addr)
{
  _page_dir_addr = addr;
}

/**
 * Set page fault address (register cr2)
 * @param addr Page Fault Address
 */
PUBLIC static inline
void
Emulation::set_page_fault_addr (Address addr)
{
  _page_fault_addr = addr;
}

/**
 * Emulate LIDT instruction
 * @param desc IDT pseudo descriptor
 */
PUBLIC static inline NEEDS["x86desc.h"]
void
Emulation::lidt (Pseudo_descriptor *desc)
{
  _idt_base  = reinterpret_cast<Idt_entry const *>(desc->base());
  _idt_limit = desc->limit();
}

/**
 * Emulate SIDT instruction
 * @param desc IDT pseudo descriptor
 */
PUBLIC static inline NEEDS["x86desc.h"]
void
Emulation::sidt (Pseudo_descriptor *desc)
{
  *desc = Pseudo_descriptor((Address)_idt_base, _idt_limit);
}

PUBLIC static inline NEEDS["x86desc.h"]
Mword
Emulation::idt_vector (Mword trap, bool user)
{
  Idt_entry const *gate = _idt_base + trap;

  // IDT limit check
  if (gate >= (_idt_base + _idt_limit))
    return 0;

  // Gate permission check
  if (user && !(gate->access() & X86desc::Access_user))
    return 0;

  assert(gate->present());

  return gate->offset();
}

PUBLIC static inline NEEDS["x86desc.h"]
bool
Emulation::idt_vector_present (Mword vector)
{
  Idt_entry const *gate = _idt_base + vector;

  if (gate >= (_idt_base + _idt_limit))
    return false;

  return gate->present();
}

PUBLIC static inline
Mword
Emulation::kernel_cs()
{
  unsigned short cs;
  asm volatile ("movw %%cs, %0" : "=g" (cs));
  return cs;
}

PUBLIC static inline
Mword
Emulation::kernel_ss()
{
  unsigned short ss;
  asm volatile ("movw %%ss, %0" : "=g" (ss));
  return ss;
}

PUBLIC static inline NEEDS [<asm/unistd.h>]
void
Emulation::modify_ldt (unsigned entry, unsigned long base_addr, unsigned limit)
{
  Ldt_user_desc ldt;

  ldt.entry_number    = entry;
  ldt.base_addr       = base_addr;
  ldt.limit           = limit;
  ldt.seg_32bit       = 1;
  ldt.contents        = 0;
  ldt.read_exec_only  = 0;
  ldt.limit_in_pages  = 0;
  ldt.seg_not_present = 0;
  ldt.useable         = 1;

  asm volatile ("int $0x80" : : "a" (__NR_modify_ldt),
                                "b" (1),
                                "c" (&ldt), "m" (ldt),
                                "d" (sizeof (ldt)));
}

PUBLIC static
void
Emulation::set_host_tls_base(unsigned b)
{
  _host_tls_base = b;
}

PUBLIC static inline
unsigned
Emulation::host_tls_base()
{ return _host_tls_base; }

PUBLIC static inline
int
Emulation::get_thread_area(Ldt_user_desc *desc, unsigned entry_number)
{
  int result;
  desc->entry_number = entry_number;

#ifndef __NR_get_thread_area
#define __NR_get_thread_area 244
#endif
  asm volatile ("int $0x80" : "=a" (result)
                            : "0" (__NR_get_thread_area),
                              "b" (desc), "m" (*desc));

  if (EXPECT_FALSE(result == -38)) // -ENOSYS
    printf("Your kernel does not support the get/set_thread_area system calls!\n"
           "The requested feature will not work.\n");
  return result;
}

PUBLIC static inline NEEDS [<asm/unistd.h>, <cassert>]
void
Emulation::thread_area_host (unsigned entry)
{
  Ldt_user_desc desc;
  int result;
  enum { NUM_TLS = 3 };
  static int called[NUM_TLS];

#ifndef __NR_set_thread_area
#define __NR_set_thread_area 243
#endif

  assert(_host_tls_base <= entry && entry < (_host_tls_base + NUM_TLS));

  if (called[entry - _host_tls_base])
    return;

  called[entry - _host_tls_base] = 1;

  result = get_thread_area(&desc, entry);
  assert(!result);

  if (!desc.base_addr || !desc.limit)
    {
      desc.base_addr       = 2;
      desc.limit           = 3;
      desc.seg_32bit       = 1;
      desc.contents        = 0;
      desc.read_exec_only  = 0;
      desc.limit_in_pages  = 0;
      desc.seg_not_present = 0;
      desc.useable         = 1;
    }

  asm volatile ("int $0x80" : "=a" (result)
                            : "0" (__NR_set_thread_area),
                              "b" (&desc), "m" (desc));

  assert(!result);
}

// ------------------------------------------------------------------------
IMPLEMENTATION[ux && mp]:

PUBLIC static inline
int
Emulation::gettid()
{
  int tid;
  asm volatile("int $0x80" : "=a" (tid) : "0" (__NR_gettid) : "memory");
  return tid;
}

PUBLIC static
int
Emulation::spawn_cpu_thread(Address ip)
{
  int result;

  asm volatile ("int $0x80         \t\n"
                "cmp $0, %%eax     \t\n"
                "jne 1f            \t\n"
                "jmp *%[ip]        \t\n"
                "1:                \t\n"
                : "=a" (result)
                : "0" (__NR_clone),
                  "b" (CLONE_VM | CLONE_FILES),
                  "c" (0),
                  "d" (0),
                  "D" (0),
                  [ip] "r" (ip)
                : "memory");

  assert(result > 0);
  return result;
}
