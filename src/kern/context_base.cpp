INTERFACE:

#include "types.h"
#include "config_tcbsize.h"
#include "fiasco_defs.h"

class Context;

class Context_base
{
public:
  enum
  {
    Size = THREAD_BLOCK_SIZE
  };

  // This virtual dtor enforces that Context / Thread / Context_base
  // all start at offset 0
  virtual ~Context_base() = 0;

protected:
  Mword _state;
};

INTERFACE [mp]:

EXTENSION class Context_base
{
private:
  friend Cpu_number current_cpu();
  Cpu_number _cpu;
};


//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "config.h"
#include "processor.h"

IMPLEMENT inline Context_base::~Context_base() {}

inline NEEDS ["config.h"]
Context *context_of(const void *ptr)
{
  return reinterpret_cast<Context *>
    (reinterpret_cast<unsigned long>(ptr) & ~(Context_base::Size - 1));
}

inline NEEDS [context_of, "processor.h"]
Context *current()
{ return context_of((void *)Proc::stack_pointer()); }


//---------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

inline
Cpu_number current_cpu()
{ return Cpu_number::boot_cpu(); }

PUBLIC inline
void
Context_base::set_current_cpu(Cpu_number)
{}

PUBLIC inline
Cpu_number
Context_base::get_current_cpu() const
{ return Cpu_number::boot_cpu(); }


//---------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "processor.h"

PUBLIC inline
void
Context_base::set_current_cpu(Cpu_number cpu)
{ _cpu = cpu; }

PUBLIC inline
Cpu_number
Context_base::get_current_cpu() const
{ return _cpu; }

inline NEEDS ["processor.h"]
Cpu_number FIASCO_PURE current_cpu()
{ return reinterpret_cast<Context_base *>(Proc::stack_pointer() & ~(Context_base::Size - 1))->_cpu; }

