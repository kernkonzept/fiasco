/**
 * Copyright (C) 2016 Kernkonzept GmbH.
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

IMPLEMENTATION [debug]:

#include "globals.h"
#include "jdb.h"
#include "kobject_helper.h"
#include "kobject_rpc.h"
#include "minmax.h"

class Jdb_object : public Kobject_h<Jdb_object, Kobject>
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
  };

  Jdb_object()
  {
    initial_kobjects.register_obj(this, Initial_kobjects::Jdb);
  }
};

JDB_DEFINE_TYPENAME(Jdb_object, "Jdb");

static Jdb_object __jdb_kobject;

extern "C" void sys_invoke_debug(Kobject_iface *o, Syscall_frame *f) __attribute__((weak));

PRIVATE inline NOEXPORT
L4_msg_tag
Jdb_object::sys_kobject_debug(L4_msg_tag tag, unsigned op,
                              L4_fpage::Rights rights,
                              Syscall_frame *f,
                              Utcb const *r_msg, Utcb *)
{
  (void)op;
  if (sys_invoke_debug)
    {
      Kobject_iface *i = Ko::deref<Kobject_iface>(&tag, r_msg, &rights);
      if (!i)
        return tag;

      sys_invoke_debug(i, f);
    }
  return commit_result(0);
}

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
      kdb_ke("tbuf_dump");
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
        tb->set(curr, curr->user_ip());

        for (unsigned i = 0; i < length; ++i)
          tb->msg.set_buf(i, str[i]);

        tb->msg.term_buf(length);
        Jdb_tbuf::commit_entry();
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
        tb->set(curr, curr->user_ip());

        for (unsigned i = 0; i < length; ++i)
          tb->set_buf(i, str[i]);

        Jdb_tbuf::commit_entry();
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
        tb->set(curr, curr->user_ip());
        tb->v[0] = access_once(&r_msg->values[1]);
        tb->v[1] = access_once(&r_msg->values[2]);
        tb->v[2] = access_once(&r_msg->values[3]);

        Mword length = min<Mword>(access_once(&r_msg->values[4]),
                                  (tag.words() - 5) * sizeof(Mword));

        auto str = reinterpret_cast<char const *>(&r_msg->values[5]);

        for (unsigned i = 0; i < length; ++i)
          tb->msg.set_buf(i, str[i]);

        tb->msg.term_buf(length);
        Jdb_tbuf::commit_entry();
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
        unsigned size = min<unsigned>(access_once(&r_msg->values[1]),
                                      (tag.words() - 2) * sizeof(Mword));

        if (size > 0)
          {
            auto txt = reinterpret_cast<char const *>(&r_msg->values[2]);
            if ((size > 2) && (txt[0] == '*') && (txt[1] == '#'))
              {
                kdb_ke_sequence(txt + 2);
                return commit_result(0);
              }

            printf("JDB-enter: '%.*s'", size, txt);
          }

        kdb_ke("user");
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


PUBLIC
L4_msg_tag
Jdb_object::kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                    Utcb const *r_msg, Utcb *s_msg)
{
  L4_msg_tag tag = f->tag();
  if ((tag.words() == 0) && (tag.items() == 0)
      && (tag.proto() == L4_msg_tag::Label_debugger))
    {
      kdb_ke("user");
      return commit_result(0);
    }

  if (!Ko::check_basics(&tag, rights, L4_msg_tag::Label_debugger))
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

    default:
      return sys_jdb(tag, op & 0xff, rights, f, r_msg, s_msg);
    }
}

