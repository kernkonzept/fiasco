INTERFACE[amd64]:

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
  Mword   _sp;
  Unsigned16 _ss, _cs;

public:
  Continuation() : _ip(~0UL) {}

  typedef Return_frame User_return_frame;

  bool valid(void const *) const
  { return _ip != ~0UL; }

  Address ip() const { return _ip; }
  void ip(Address ip) { _ip = ip; }

  Mword flags(Return_frame const *) const { return _flags; }
  void flags(Return_frame *, Mword flags) { _flags = flags; }

  Mword sp(Return_frame const *) const { return _sp; }
  void sp(Return_frame *, Mword sp) { _sp = sp; }

  void save(Return_frame const *regs)
  {
    _ip  = regs->ip();
    _flags = regs->flags();
    _sp = regs->sp();
    _ss = regs->ss();
    _cs = regs->cs() & ~0x80;
  }

  void activate(Return_frame *regs, void *cont_func)
  {
    save(regs);
    regs->ip(Mword(cont_func));
    // interrupts must stay off, do not singlestep in kernel code
    regs->flags(regs->flags() & ~(EFLAGS_TF | EFLAGS_IF));
    regs->sp((Address)(regs + 1));
    regs->ss(Gdt::gdt_data_kernel | Gdt::Selector_kernel);
    regs->cs(Gdt::gdt_code_kernel | Gdt::Selector_kernel);
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
    regs->ip(_ip);
    regs->flags(_flags);
    regs->sp(_sp);
    regs->ss(_ss);
    regs->cs(_cs);
    clear();
  }

};

