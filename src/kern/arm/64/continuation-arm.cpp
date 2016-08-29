INTERFACE[arm && 64bit]:

#include "entry_frame.h"
#include "types.h"

class Continuation
{
public:
  bool valid(Return_frame const *regs) const
  { return regs->eret_work; }

  Mword flags(Return_frame const *r) const { return r->pstate; }
  void flags(Return_frame *r, Mword status) { r->pstate = status; }

  Mword sp(Return_frame const *o) const { return o->usp; }
  void sp(Return_frame *o, Mword sp) { o->usp = sp; }

  void activate(Return_frame *regs, void *cont_func)
  {
    // we use $0 (the zero register for flagging eret work)
    regs->eret_work = (Mword)cont_func;
  }

  void clear(Return_frame *regs)
  { regs->eret_work = 0; }

  void restore(Return_frame *regs)
  { regs->eret_work = 0; }

};

