/**
 * Copyright (C) 2016, 2020-2021, 2023-2024 Kernkonzept GmbH.
 * Author: Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 *
 * Jdb kernel object.
 *
 * Common entry path for interaction of l4re applications with the kernel
 * debugger. Have a look at l4sys/include/kdebug.h and l4sys/include/ktrace.h
 * for reference.
 */

IMPLEMENTATION [rt_dbg || cov]:

#include "globals.h"
#include "kobject_helper.h"

class Jdb_object : public Kobject_h<Jdb_object, Kobject>
{
private:
  L4_msg_tag sys_print_cov_data();
public:
  Jdb_object()
  {
    initial_kobjects->register_obj(this, Initial_kobjects::Jdb);
  }

  L4_msg_tag
  kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
          Utcb const *r_msg, Utcb *s_msg);
};

IMPLEMENT_DEFAULT
L4_msg_tag
Jdb_object::kinvoke(L4_obj_ref, L4_fpage::Rights, Syscall_frame *,
                    Utcb const *, Utcb *)
{
  return commit_result(-L4_err::ENosys);
}


//----------------------------------------------------------------------------
IMPLEMENTATION [rt_dbg]:

#include "kdb_ke.h"
#include "kobject_rpc.h"
#include "minmax.h"
#include "global_data.h"

EXTENSION class Jdb_object
{
public:
  enum
  {
    Jdb_enter      = 0,
    Jdb_outchar    = 1,
    Jdb_outnstring = 2,
    Jdb_outhex32   = 3,
    Jdb_outhex20   = 4,
    Jdb_outhex16   = 5,
    Jdb_outhex12   = 6,
    Jdb_outhex8    = 7,
    Jdb_outdec     = 8,

    // 0x200 prefix for tbuf opcodes
    Tbuf_log       = 1,
    Tbuf_clear     = 2,
    Tbuf_dump      = 3,
    Tbuf_log_3val  = 4,
    Tbuf_log_bin   = 5,

    // 0x500 prefix for dump opcodes
    Dump_kmem_stats = 0,
  };
};

JDB_DEFINE_TYPENAME(Jdb_object, "Jdb");

static DEFINE_GLOBAL Global_data<Jdb_object> __jdb_kobject;

extern "C" void sys_invoke_debug(Kobject_iface *o, Syscall_frame *f) __attribute__((weak));

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_kobject_debug(L4_msg_tag tag, unsigned /* op */,
                              L4_fpage::Rights rights,
                              Syscall_frame *f,
                              Utcb const *r_msg, Utcb *)
{
  if (sys_invoke_debug)
    {
      Kobject_iface *i = Ko::deref<Kobject_iface>(&tag, r_msg, &rights);
      if (!i)
        return tag;

      sys_invoke_debug(i, f);
      return f->tag();
    }
  return commit_result(0);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [rt_dbg && debug]:

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_tbuf(L4_msg_tag tag, unsigned op,
                     L4_fpage::Rights,
                     Syscall_frame *,
                     Utcb const *r_msg, Utcb *)
{
  Thread *curr = current_thread();
  switch (op)
    {
    case Tbuf_clear:
      Jdb_tbuf::clear_tbuf();
      return commit_result(0);

    case Tbuf_dump:
      kdb_ke_sequence("*tbufdumpgzip_mode_", 19);
      return commit_result(0);

    case Tbuf_log:
      {
        if (tag.words() < 2)
          return commit_result(-L4_err::EMsgtooshort);

        // value[1] == length, set_buf will ensure it fits into Tb_entry_ke
        //             however, we must not read above utcb
        // value[2] == start of payload
        Mword length = access_once(&r_msg->values[1]);
        // restrict length to words in utcb
        length = min<Mword>((tag.words() - 2) * sizeof(Mword), length);

        auto str = reinterpret_cast<char const *>(&r_msg->values[2]);
        Tb_entry_ke *tb = Jdb_tbuf::new_entry<Tb_entry_ke>();
        tb->set(curr, curr->regs()->ip_syscall_user());

        for (unsigned i = 0; i < length; ++i)
          tb->msg.set_buf(i, str[i]);

        tb->msg.term_buf(length);
        Jdb_tbuf::commit_entry(tb);
        return commit_result(0);
      }

    case Tbuf_log_bin:
      {
        if (tag.words() < 2)
          return commit_result(-L4_err::EMsgtooshort);

        // value[1] == length of payload in bytes, *must not* be bigger than
        //             Tb_entry_ke_bin::SIZE and we must not read above utcb
        // value[2] == start of payload
        Mword length = min<Mword>(access_once(&r_msg->values[1]),
                                  Tb_entry_ke_bin::SIZE,
                                  (tag.words() - 2) * sizeof(Mword));

        auto str = reinterpret_cast<char const *>(&r_msg->values[2]);
        Tb_entry_ke_bin *tb = Jdb_tbuf::new_entry<Tb_entry_ke_bin>();
        tb->set(curr, curr->regs()->ip_syscall_user());

        for (unsigned i = 0; i < length; ++i)
          tb->set_buf(i, str[i]);

        Jdb_tbuf::commit_entry(tb);
        return commit_result(0);
      }

    case Tbuf_log_3val:
      {
         if (tag.words() < 5)
          return commit_result(-L4_err::EMsgtooshort);

        // values[1] == payload 1
        // values[2] == payload 2
        // values[3] == payload 3
        // values[4] == length of string in bytes, set_buf ensures it fits into msg buffer,
        //              but we must not read above utcb
        // values[5] == string
        Tb_entry_ke_reg *tb = Jdb_tbuf::new_entry<Tb_entry_ke_reg>();
        tb->set(curr, curr->regs()->ip_syscall_user());
        tb->v[0] = access_once(&r_msg->values[1]);
        tb->v[1] = access_once(&r_msg->values[2]);
        tb->v[2] = access_once(&r_msg->values[3]);

        Mword length = min<Mword>(access_once(&r_msg->values[4]),
                                  (tag.words() - 5) * sizeof(Mword));

        auto str = reinterpret_cast<char const *>(&r_msg->values[5]);

        for (unsigned i = 0; i < length; ++i)
          tb->msg.set_buf(i, str[i]);

        tb->msg.term_buf(length);
        Jdb_tbuf::commit_entry(tb);
        return commit_result(0);
      }

    default:
      return commit_result(-L4_err::ENosys);
    }
}

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_jdb(L4_msg_tag tag, unsigned op,
                    L4_fpage::Rights,
                    Syscall_frame *,
                    Utcb const *r_msg, Utcb *)
{
  if (tag.words() < 2)
    return commit_result(-L4_err::EMsgtooshort);

  switch (op)
    {
    case Jdb_enter:
      {
        // On some architectures, 'r_msg' is not accessible from JDB context.
        // Thus pass the pointer of the kernel context to kdb_ke_*().
        Utcb const *u_kern = current()->utcb().kern();

        auto length = min<unsigned>(access_once(&r_msg->values[1]),
                                    (tag.words() - 2) * sizeof(Mword));
        auto txt_user = reinterpret_cast<char const *>(&r_msg->values[2]);
        auto txt_kern = reinterpret_cast<char const *>(&u_kern->values[2]);

        if (length > 2 && txt_user[0] == '*' && txt_user[1] == '#')
          kdb_ke_sequence(txt_kern + 2, length);
        else
          kdb_ke_nstr(txt_kern, length);

        return commit_result(0);
      }
    case Jdb_outchar:
      putchar(r_msg->values[1]);
      return commit_result(0);

    case Jdb_outnstring:
      {
        int length = min<unsigned>(access_once(&r_msg->values[1]),
                                   (tag.words() - 2) * sizeof(Mword));
        printf("%.*s", length, reinterpret_cast<char const *>(&r_msg->values[2]));
        return commit_result(0);
      }
    case Jdb_outhex32:
      printf("%08lx", r_msg->values[1] & 0xffffffff);
      return commit_result(0);

    case Jdb_outhex20:
      printf("%05lx", r_msg->values[1] & 0xfffff);
      return commit_result(0);

    case Jdb_outhex16:
      printf("%04lx", r_msg->values[1] & 0xffff);
      return commit_result(0);

    case Jdb_outhex12:
      printf("%03lx", r_msg->values[1] & 0xfff);
      return commit_result(0);

    case Jdb_outhex8:
      printf("%02lx", r_msg->values[1] & 0xff);
      return commit_result(0);

    case Jdb_outdec:
      printf("%lu", r_msg->values[1]);
      return commit_result(0);

    default:
      return commit_result(-L4_err::ENosys);
    }
}

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_debugger(L4_msg_tag,
                         L4_fpage::Rights,
                         Syscall_frame *,
                         Utcb const *, Utcb *)
{
  kdb_ke("user");
  return commit_result(0);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [rt_dbg && !debug]:

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_tbuf(L4_msg_tag, unsigned,
                     L4_fpage::Rights,
                     Syscall_frame *,
                     Utcb const *, Utcb *)
{
  return commit_result(-L4_err::ENosys);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_jdb(L4_msg_tag, unsigned,
                    L4_fpage::Rights,
                    Syscall_frame *,
                    Utcb const *, Utcb *)
{
  return commit_result(-L4_err::ENosys);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_debugger(L4_msg_tag,
                         L4_fpage::Rights,
                         Syscall_frame *,
                         Utcb const *, Utcb *)
{
  return commit_result(-L4_err::ENosys);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [rt_dbg]:

IMPLEMENT_OVERRIDE
L4_msg_tag
Jdb_object::kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                    Utcb const *r_msg, Utcb *s_msg)
{
  L4_msg_tag tag = f->tag();
  if ((tag.words() == 0) && (tag.items() == 0)
      && (tag.proto() == L4_msg_tag::Label_debugger))
    return sys_debugger(tag, rights, f, r_msg, s_msg);

  if (!Ko::check_basics(&tag, L4_msg_tag::Label_debugger))
    return tag;

  unsigned op =  access_once(&r_msg->values[0]);
  auto group = op >> 8;
  switch (group)
    {
    case 1:
      //XXX: there must be a better way...
      current_thread()->utcb().access()->values[0] &= 0xff;
      return sys_kobject_debug(tag, op & 0xff, rights, f, r_msg, s_msg);

    case 2:
      return sys_tbuf(tag, op & 0xff, rights, f, r_msg, s_msg);

    case 4:
      return sys_print_cov_data();

    case 5:
      return sys_print_kmem_stats(op & 0xff);

    default:
      return sys_jdb(tag, op & 0xff, rights, f, r_msg, s_msg);
    }
}

//------------------------------------------------------------------
IMPLEMENTATION [cov]:

extern "C" void cov_print(void) __attribute__ ((weak));

IMPLEMENT
L4_msg_tag
Jdb_object::sys_print_cov_data()
{
  cov_print();
  return commit_result(0);
}

//------------------------------------------------------------------
IMPLEMENTATION [rt_dbg && !cov]:

IMPLEMENT
L4_msg_tag
Jdb_object::sys_print_cov_data()
{
  return commit_result(-L4_err::ENosys);
}

//------------------------------------------------------------------
IMPLEMENTATION [rt_dbg && jdb_kmem_stats]:

#include "kmem_alloc.h"

PRIVATE static
L4_msg_tag
Jdb_object::sys_print_kmem_stats(unsigned op)
{
  switch (op)
    {
    case Dump_kmem_stats:
      Kmem_alloc::allocator()->debug_dump();
      return commit_result(0);

    default:
      return commit_result(-L4_err::ENosys);
    }
}

//------------------------------------------------------------------
IMPLEMENTATION [rt_dbg && !jdb_kmem_stats]:

PRIVATE static
L4_msg_tag
Jdb_object::sys_print_kmem_stats(unsigned)
{
  return commit_result(-L4_err::ENosys);
}


//------------------------------------------------------------------
IMPLEMENTATION [!rt_dbg && cov]:

#include "kobject_rpc.h"

JDB_DEFINE_TYPENAME(Jdb_object, "Jdb");

static Jdb_object __jdb_kobject;

IMPLEMENT_OVERRIDE
L4_msg_tag
Jdb_object::kinvoke(L4_obj_ref, L4_fpage::Rights, Syscall_frame *f,
                    Utcb const *r_msg, Utcb *)
{
  L4_msg_tag tag = f->tag();

  if (!Ko::check_basics(&tag, L4_msg_tag::Label_debugger))
    return tag;

  unsigned op =  access_once(&r_msg->values[0]);
  auto group = op >> 8;
  switch (group)
    {
    case 4:
      return sys_print_cov_data();

    default:
      return commit_result(-L4_err::ENosys);
    }
}
