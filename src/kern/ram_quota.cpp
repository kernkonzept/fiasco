INTERFACE:

#include <cstddef>

class Ram_quota
{
public:
  static Ram_quota *root;
  virtual ~Ram_quota() = 0;

private:
  Ram_quota *_parent;
  unsigned long _current;
  unsigned long _max;
};


IMPLEMENTATION:

#include "cpu_lock.h"
#include "lock_guard.h"

Ram_quota *Ram_quota::root;

IMPLEMENT inline Ram_quota::~Ram_quota() {}

PUBLIC inline NEEDS[<cstddef>]
void *
Ram_quota::operator new (size_t, void *b) throw()
{ return b; }

PUBLIC
Ram_quota::Ram_quota()
  : _parent(0), _current(0), _max (~0UL)
{
  root = this;
}

PUBLIC
Ram_quota::Ram_quota(Ram_quota *p, unsigned long max)
  : _parent(p), _current(0), _max (max)
{}

PUBLIC
unsigned long
Ram_quota::current() const
{ return _current; }

PUBLIC
bool
Ram_quota::alloc(signed long bytes)
{ 
  auto guard = lock_guard(cpu_lock);
  if (unlimited() || _current + bytes <= _max)
    {
      _current += bytes;
      return true;
    }

  return false;
}

PUBLIC inline NEEDS[Ram_quota::alloc]
void
Ram_quota::free(signed long bytes)
{ alloc(-bytes); }


PUBLIC inline
Ram_quota*
Ram_quota::parent() const
{ return _parent; }

PUBLIC inline
unsigned long
Ram_quota::limit() const
{ return _max; }

PUBLIC inline
bool
Ram_quota::unlimited() const
{ return _max == ~0UL; }

