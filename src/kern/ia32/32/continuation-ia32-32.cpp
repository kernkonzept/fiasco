INTERFACE[ia32]:

#include "entry_frame.h"
#include "gdt.h"
#include "member_offs.h"
#include "regdefs.h"
#include "types.h"

class Continuation
{
  MEMBER_OFFSET();

private:
  Address _ip;
  Mword   _flags;

public:
  Continuation() : _ip(~0UL) {}

  typedef Return_frame User_return_frame;

  bool valid(void const *) const
  { return _ip != ~0UL; }

  Address ip() const { return _ip; }
  void ip(Address ip) { _ip = ip; }

  Mword flags(Return_frame const *) const { return _flags; }
  void flags(Return_frame *, Mword flags) { _flags = flags; }

  Mword sp(Return_frame const *o) const { return o->sp(); }
  void sp(Return_frame *o, Mword sp) { o->sp(sp); }

  void save(Return_frame const *regs)
  {
    _ip  = regs->ip();
    _flags = regs->flags();
    // LOG_MSG_3VAL(current(), "sav", _ip, _flags, 0);
  }

  void activate(Return_frame *regs, void *cont_func)
  {
    save(regs);
    regs->ip(Mword(cont_func));
    // interrupts must stay off, do not singlestep in kernel code
    regs->flags(regs->flags() & ~(EFLAGS_TF | EFLAGS_IF));
    regs->cs(Gdt::gdt_code_kernel | Gdt::Selector_kernel);
    // LOG_MSG_3VAL(current(), "act", regs->ip(), regs->flags(), regs->cs());
  }

  void set(Return_frame *dst, User_return_frame const *src)
  {
    _ip = src->ip();
    _flags = src->flags();
    dst->sp(src->sp());
  }

  void get(User_return_frame *dst, Return_frame const *src) const
  {
    dst->ip(_ip);
    dst->flags(_flags);
    dst->sp(src->sp());
  }

  void clear() { _ip = ~0UL; }

  void restore(Return_frame *regs)
  {
    // LOG_MSG_3VAL(current(), "rst", _ip, _flags, 0);
    regs->ip(_ip);
    regs->flags(_flags);
    regs->cs(Gdt::gdt_code_user | Gdt::Selector_user);
    clear();
  }

};

