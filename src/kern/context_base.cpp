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

/**
 * Get the current context.
 *
 * Derive the current context from the stack pointer. The exact value of the
 * address is not important as it only needs to identify the stack of the
 * current context.
 *
 * During context switch, all registers are either clobbered or saved and
 * restored, ensuring the right value is derived in the new context.
 *
 * \return Current context.
 */
inline NEEDS [context_of, "processor.h"]
Context *current()
{
#if FIASCO_HAS_BUILTIN(__builtin_stack_address)
  // Optimized version of the textbook approach of manually accessing the
  // platform-specific stack pointer. Since the compiler understands the
  // semantics of the intrinsic function used, the call can be possibly
  // optimized out if not needed and the return value can be cached if
  // used multiple times.
  return context_of(__builtin_stack_address());
#else
  // Compatibility variant for compilers that do not provide the intrinsic
  // function to read the stack pointer.
  return context_of(reinterpret_cast<void *>(Proc::stack_pointer_for_context()));
#endif
}


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

inline NEEDS [current, "processor.h"]
Cpu_number FIASCO_PURE current_cpu()
{ return reinterpret_cast<Context_base *>(current())->_cpu; }

