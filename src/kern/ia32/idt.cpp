/*
 * Fiasco Interrupt Descriptor Table (IDT) Code
 */

INTERFACE:

#include "initcalls.h"
#include "idt_init.h"
#include "kmem.h"
#include "mem_layout.h"
#include "types.h"
#include "x86desc.h"

class Idt_init_entry;

class Idt
{
  friend class Jdb_kern_info_bench;

public:
  static constexpr unsigned _idt_max = FIASCO_IDT_MAX;

private:
  static constexpr Address _idt = Mem_layout::Idt;
  static Address _idt_pa;
};

IMPLEMENTATION:

#include <cassert>
#include "gdt.h"
#include "irq_chip.h"
#include "mem_unit.h"
#include "paging.h"
#include "panic.h"
#include "kmem_alloc.h"
#include "vmem_alloc.h"
#include <cstring>

Address Idt::_idt_pa;

/**
 * IDT write-protect/write-unprotect function.
 * @param writable true if IDT should be made writable, false otherwise
 */
PRIVATE static
void
Idt::set_writable(bool writable)
{
  auto e = Kmem::current_cpu_kdir()->walk(Virt_addr(_idt));

  // Make sure page directory entry is valid and not a 4MB page
  assert(e.is_valid() && e.level == Pdir::Depth);

  if (writable)
    e.add_attribs(Page::Attr::writable()); // Make read-write
  else
    e.del_attribs(Page::Attr::writable()); // Make read-only

  Mem_unit::tlb_flush_kernel(_idt);
}

PUBLIC static FIASCO_INIT
void
Idt::init_table(Idt_init_entry *src, Idt_entry *idt)
{
  while (src->entry)
    {
      assert(src->vector < _idt_max);

      auto type = static_cast<Idt_entry::Type_system>(src->type & 0x1f);
      auto dpl = static_cast<Idt_entry::Dpl>((src->type >> 5) & 0x03);

      if (type == Idt_entry::Task_gate)
        {
          // Task gate
          idt[src->vector] = Idt_entry(src->entry, dpl);
        }
      else
        {
          // Interrupt/trap gate
          idt[src->vector] = Idt_entry(src->entry, Gdt::gdt_code_kernel, type,
                                       dpl);
        }

      src++;
    }
}

/**
 * IDT initialization function. Sets up initial interrupt vectors.
 * It also write-protects the IDT because of the infamous Pentium F00F bug.
 */
PUBLIC static FIASCO_INIT
void
Idt::init()
{
  assert (_idt_max * sizeof(Idt_entry) <= Config::PAGE_SIZE && "IDT too large");
  auto alloc = Kmem_alloc::allocator();
  Idt_entry *idt = static_cast<Idt_entry *>(alloc->alloc(Config::page_size()));
  if (!idt)
    panic("IDT allocation failure: %d", __LINE__);

  _idt_pa = Mem_layout::pmem_to_phys(idt);
  memset(static_cast<void*>(idt), 0, Config::PAGE_SIZE);

  init_table(reinterpret_cast<Idt_init_entry*>(&idt_init_table), idt);

  init_current_cpu();
}

PUBLIC static
void
Idt::init_current_cpu()
{
  auto d = Kmem::current_cpu_kdir()->walk(Virt_addr(_idt), Pdir::Depth);
  if (d.level != Pdir::Depth)
    panic("IDT allocation failure: %d: level=%d %lx", __LINE__,
          d.level, *d.pte);

  if (!d.is_valid())
    d.set_page(Phys_mem_addr(_idt_pa),
               Page::Attr(Page::Rights::R(), Page::Type::Normal(),
                          Page::Kern::Global(), Page::Flags::Referenced()));

  if (d.page_addr() != _idt_pa)
    panic("IDT allocation failure: %d: some other page mapped here %lx",
          __LINE__, _idt);

  load();
}


PUBLIC static
void
Idt::load()
{
  Pseudo_descriptor desc(_idt, _idt_max*sizeof(Idt_entry)-1);
  set(&desc);
}

PUBLIC static
void
Idt::set_entry(unsigned vector, Idt_entry entry)
{
  assert (vector < _idt_max);

  set_writable(true);

  Idt_entry *entries = reinterpret_cast<Idt_entry*>(_idt);
  entries[vector] = entry;
  set_writable(false);
}

PUBLIC static
Idt_entry const &
Idt::get(unsigned vector)
{
  assert (vector < _idt_max);

  return reinterpret_cast<Idt_entry*>(_idt)[vector];
}

/**
 * IDT patching function.
 *
 * Allows to change interrupt gate vectors at runtime. It makes the IDT
 * writable for the duration of this operation.
 *
 * \param vector  Interrupt vector to be modified.
 * \param addr    Address of the new interrupt handler for the vector.
 * \param user    True if user mode can use this vector, false otherwise.
 */
PUBLIC static
void
Idt::set_entry(unsigned vector, Address addr, bool user)
{
  assert(vector < _idt_max);

  set_writable(true);

  Idt_entry *entries = reinterpret_cast<Idt_entry *>(_idt);

  if (addr)
    entries[vector] = Idt_entry(addr, Gdt::gdt_code_kernel,
                                Idt_entry::Intr_gate,
                                (user ? Idt_entry::User : Idt_entry::Kernel));
  else
    entries[vector] = Idt_entry();

  set_writable(false);
}

PUBLIC static
Address
Idt::get_entry(unsigned vector)
{
  assert (vector < _idt_max);
  Idt_entry *entries = reinterpret_cast<Idt_entry*>(_idt);
  return entries[vector].offset();
}

PUBLIC static inline
Address
Idt::idt()
{
  return _idt;
}


//---------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include "config.h"

/**
 * IDT loading function.
 * Loads IDT base and limit into the CPU.
  * @param desc IDT descriptor (base address, limit)
  */  
PUBLIC static inline
void
Idt::set(Pseudo_descriptor *desc)
{
  asm volatile ("lidt %0" : : "m" (*desc));
}

PUBLIC static inline
void
Idt::get(Pseudo_descriptor *desc)
{
  asm volatile ("sidt %0" : "=m" (*desc) : : "memory");
}

extern "C" void entry_int_timer();
extern "C" void entry_int_timer_slow();
extern "C" void entry_int7();
extern "C" void entry_intf();
extern "C" void entry_int_pic_ignore();

/**
 * Set IDT vector to the normal timer interrupt handler.
 */
PUBLIC static
void
Idt::set_vectors_run()
{
  Address func =
    (Config::esc_hack || Config::watchdog ||
     Config::serial_esc==Config::SERIAL_ESC_NOIRQ)
    ? reinterpret_cast<Address>(entry_int_timer_slow) // slower for debugging
    : reinterpret_cast<Address>(entry_int_timer);     // non-debugging

  set_entry(Config::scheduler_irq_vector, func, false);
#if 0
  if (!Irq_chip::hw_chip->is_free(0x7))
    Irq_chip::hw_chip->reset(0x07);

  if (!Irq_chip::hw_chip->is_free(0xf))
    Irq_chip::hw_chip->reset(0x0f);
#endif
}
