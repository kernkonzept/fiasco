INTERFACE[riscv]:

#include "entry_frame.h"
#include "types.h"

class Continuation
{
public:
  bool valid(Return_frame const *regs) const
  {
    return regs->eret_work;
  }

  void activate(Return_frame *regs, void *cont_func)
  {
    regs->eret_work = reinterpret_cast<Mword>(cont_func);
  }

  void clear(Return_frame *regs)
  {
    regs->eret_work = 0;
  }

  void restore(Return_frame *regs)
  {
    regs->eret_work = 0;
  }
};
