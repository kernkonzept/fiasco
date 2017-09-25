INTERFACE [ia32 || amd64]:

#include "spin_lock.h"

EXTENSION class Space
{
protected:
  class Ldt
  {
  public:
    Ldt() : _addr(0), _size(0) {}
    Address addr() const { return (Address)_addr; }
    Mword   size() const { return _size; }

    void size(Mword);
    void alloc();

    ~Ldt();

  private:
    void *_addr;
    Mword _size;
  };

  friend class Jdb_misc_debug;

  Ldt _ldt;
};

// ---------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include "cpu.h"
#include "globals.h"
#include "mem.h"

IMPLEMENT inline
void
Space::Ldt::size(Mword size)
{ _size = size; }

IMPLEMENT inline NEEDS["mem.h"]
void
Space::Ldt::alloc()
{
  // LDT maximum size is one page
  _addr = Kmem_alloc::allocator()->alloc(Config::PAGE_SHIFT);
  Mem::memset_mwords(reinterpret_cast<void *>(addr()), 0,
                     Config::PAGE_SIZE / sizeof(Mword));
}

IMPLEMENT inline
Space::Ldt::~Ldt()
{
  if (addr())
    Kmem_alloc::allocator()->free(Config::PAGE_SHIFT,
                                  reinterpret_cast<void*>(addr()));
}

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h", "globals.h"]
void
Space::switchin_context(Space *from)
{
  if (this != from)
    {
      Mem_space::switchin_context(from);
      Cpu::cpus.cpu(current_cpu()).enable_ldt(_ldt.addr(), _ldt.size());
    }
}
