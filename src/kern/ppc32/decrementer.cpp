INTERFACE:

#include "types.h"

class Decrementer
{
public:
  void init(unsigned long interval);
  inline void set();
  inline void enable();
  inline void disable();
  Decrementer() : _interval(0), _enabled(false) {}

private:
  Unsigned32 _interval;
  bool _enabled;
};


//------------------------------------------------------------------------------
IMPLEMENTATION[ppc32]:

PUBLIC static inline
Decrementer *
Decrementer::d()
{
  static Decrementer _kernel_decr;
  return &_kernel_decr;
}

PRIVATE inline
void
Decrementer::set2(unsigned long interval)
{
  if (!_enabled && interval)
    return;

  asm volatile("mtdec %0" : : "r"(interval) : "memory");
}

IMPLEMENT
void
Decrementer::init(unsigned long interval)
{
  _interval = interval;
}

IMPLEMENT inline NEEDS[Decrementer::set2]
void
Decrementer::set()
{
  set2(_interval);
}

IMPLEMENT inline
void
Decrementer::enable()
{
  _enabled = true;
  set();
}

IMPLEMENT inline
void
Decrementer::disable()
{
  _enabled = false;
  set2(0);
}
