INTERFACE[arm]:

#include "types.h"
#include "entry_frame.h"
#include "member_offs.h"
#include "processor.h"

class Continuation
{
  MEMBER_OFFSET();

private:
  Address _ip;
  Mword   _psr;

public:
  Continuation() : _ip(~0UL) {}

  typedef Return_frame User_return_frame;

  bool valid(void const *) const
  { return _ip != ~0UL; }

  Address ip() const { return _ip; }
  void ip(Address ip) { _ip = ip; }

  Mword flags(Return_frame const *) const { return _psr; }
  void flags(Return_frame *, Mword psr) { _psr = psr; }

  Mword sp(Return_frame const *o) const { return o->usp; }
  void sp(Return_frame *o, Mword sp) { o->usp = sp; }

  void save(Return_frame const *regs)
  {
    _ip  = regs->ip();
    _psr = regs->psr;
  }

  void activate(Return_frame *regs, void *cont_func)
  {
    save(regs);
    regs->pc = Mword(cont_func);
    regs->psr &= ~(Proc::Status_mode_mask | Proc::Status_thumb); // clear mode
    regs->psr |= Proc::Status_mode_supervisor | Proc::Status_interrupts_disabled;
  }

  void set(Return_frame *dst, User_return_frame const *src)
  {
    dst->usp = src->usp;
    dst->ulr = src->ulr;
    dst->km_lr = src->km_lr;
    _ip = src->pc;
    _psr = src->psr;
  }

  void get(User_return_frame *dst, Return_frame const *src) const
  {
    dst->usp = src->usp;
    dst->ulr = src->ulr;
    dst->km_lr = src->km_lr;
    dst->pc = _ip;
    dst->psr = _psr;
  }

  void clear() { _ip = ~0UL; }

  void restore(Return_frame *regs)
  {
    regs->pc = _ip;
    regs->psr = _psr;
    clear();
  }

};

