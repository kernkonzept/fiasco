INTERFACE:

#include "icu_helper.h"

class Irq;

class Vlog :
  public cxx::Dyn_castable<Vlog, Icu_h<Vlog> >,
  public Irq_chip_virt<1>
{
public:
  enum O_flags
  {
    F_ONLCR  = 000004, ///< Map NL to CR-NL on output
    F_OCRNL  = 000010, ///< Map CR to NL on output
    F_ONLRET = 000040, ///< Do not ouput CR
  };

  enum I_flags
  {
    F_INLCR = 000100,
    F_IGNCR = 000200,
    F_ICRNL = 000400,
  };

  enum L_flags
  {
    F_ECHO = 000010,
  };

  enum class Op : Mword
  {
    Write    = 0,
    Read     = 1,
    Set_attr = 2,
    Get_attr = 3,
  };

private:
  Mword _i_flags;
  Mword _o_flags;
  Mword _l_flags;
};

IMPLEMENTATION:

#include "entry_frame.h"
#include "map_util.h"
#include "mem_space.h"
#include "l4_buf_iter.h"
#include "thread.h"
#include "vkey.h"
#include "irq.h"
#include "irq_controller.h"
#include "global_data.h"

JDB_DEFINE_TYPENAME(Vlog, "Vlog");

PUBLIC
Vlog::Vlog()
: _i_flags(F_ICRNL), _o_flags(F_ONLCR), _l_flags(F_ECHO)
{
  Vkey::irq(icu_irq_ptr(0));
  Vkey::set_echo(Vkey::Echo_crnl);
  initial_kobjects->register_obj(this, Initial_kobjects::Log);
}

PUBLIC void
Vlog::operator delete (void *)
{
  printf("WARNING: tried to delete kernel Vlog object.\n");
}


PRIVATE inline NOEXPORT
void
Vlog::log_string(Utcb const *u)
{
  unsigned len = u->values[1];
  char const *str = reinterpret_cast<char const *>(&u->values[2]);

  if (len > sizeof(u->values) - sizeof(u->values[0]) * 2)
    return;

  while (len--)
    {
      int c = *str++;

      // the kernel does this anyway
#if 0
      if (_o_flags & F_ONLCR && c == '\n')
	putchar('\r');
#endif

      if (_o_flags & F_OCRNL && c == '\r')
	c = '\n';

      if (_o_flags & F_ONLRET && c == '\r')
	continue;

      putchar(c);
    }
}

PRIVATE inline NOEXPORT
L4_msg_tag
Vlog::get_input(L4_fpage::Rights rights, Syscall_frame*, Utcb *u)
{
  if (!have_receive(u))
    return commit_result(0);

  if (!(rights & L4_fpage::Rights::CW()))
    return commit_result(-L4_err::EPerm);

  char *buffer = reinterpret_cast<char *>(&u->values[1]);
  long cnt_down = min<Mword>(u->values[0] >> 16,
                             sizeof(u->values) - sizeof(u->values[0]));
  int i;
  while ((i = Vkey::get()) != -1 && cnt_down)
    {
      Vkey::clear();

      if (_i_flags & F_INLCR && i == '\n')
	i = '\r';

      if (_i_flags & F_IGNCR && i == '\r')
	continue;

      if (_i_flags & F_ICRNL && i == '\r')
	i = '\n';

      *buffer = i;
      ++buffer;
      --cnt_down;
    }

  u->values[0] = buffer - reinterpret_cast<char *>(&u->values[1]);
  if (i == -1)
    u->values[0] |= 1UL<<31;
  return commit_result(0);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Vlog::set_attr(L4_fpage::Rights, Syscall_frame const *, Utcb const *u)
{
  _i_flags = u->values[1];
  _o_flags = u->values[2] | F_ONLCR;
  _l_flags = u->values[3];
  Vkey::set_echo((!(_l_flags & F_ECHO))
                  ? Vkey::Echo_off
                  : (_o_flags & F_OCRNL
                    ? Vkey::Echo_crnl
                    : Vkey::Echo_on));

  return commit_result(0);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Vlog::get_attr(L4_fpage::Rights, Syscall_frame *, Utcb *u)
{
  if (!have_receive(u))
    return commit_result(0);

  u->values[1] = _i_flags;
  u->values[2] = _o_flags;
  u->values[3] = _l_flags;
  return commit_result(0, 4);
}

PUBLIC
L4_msg_tag
Vlog::kinvoke(L4_obj_ref ref, L4_fpage::Rights rights, Syscall_frame *f,
              Utcb const *r_msg, Utcb *s_msg)
{
  L4_msg_tag tag = f->tag();

  if (tag.proto() == L4_msg_tag::Label_irq)
    {
      if (Icu_h_base::Op{r_msg->values[0]} == Icu_h_base::Op::Bind
          && !Vkey::receive_enabled())
        WARN("Without -esc / -serial_esc, Vlog will not generate input events!\n");

      return Icu_h<Vlog>::icu_invoke(ref, rights, f, r_msg, s_msg);
    }

  if (!Ko::check_basics(&tag, L4_msg_tag::Label_log))
    return tag;

  switch (Op{r_msg->values[0]})
    {
    case Op::Write:
      log_string(r_msg);
      return no_reply();
    case Op::Set_attr:
      return set_attr(rights, f, r_msg);
    case Op::Get_attr:
      return get_attr(rights, f, s_msg);
    default: // Op::Read
      return get_input(rights, f, s_msg);
    }
}


static DEFINE_GLOBAL Global_data<Vlog> __vlog;

