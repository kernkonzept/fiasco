INTERFACE[mips]:

#include "entry_frame.h"
#include "types.h"

class Continuation
{
public:
  bool valid(Return_frame const *regs) const
  { return regs->r[0]; }

  Mword flags(Return_frame const *r) const { return r->status; }
  void flags(Return_frame *r, Mword status) { r->status = status; }

  Mword sp(Return_frame const *o) const { return o->r[Return_frame::R_sp]; }
  void sp(Return_frame *o, Mword sp) { o->r[Return_frame::R_sp] = sp; }

  void activate(Return_frame *regs, void *cont_func)
  {
    // we use $0 (the zero register for flagging eret work)
    regs->eret_work((Mword)cont_func);
  }

  void clear(Return_frame *regs) { regs->r[0] = 0; }

  void restore(Return_frame *regs)
  {
    regs->eret_work(0);
  }

};

