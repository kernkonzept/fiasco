// vi:ft=cpp

/*
 * Copyright (C) 2015 Kernkonzept GmbH.
 * Author: Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <cxx/type_traits>
#include <l4_types.h>
#include <l4_buf_iter.h>

namespace Ko {

/// import L4_fpage::Rights into Ko namespace
typedef L4_fpage::Rights Rights;

/// create an L4_msg_tag with the given return code
inline L4_msg_tag
commit_result(Mword error, unsigned words = 0, unsigned items = 0)
{ return L4_msg_tag(words, items, 0, error); }

/**
 * create an L4_msg_tag with IPC error flag set and write the given IPC
 * error code into the UTCB error register.
 */
inline L4_msg_tag
commit_error(Utcb const *utcb, L4_error e,
             L4_msg_tag tag = L4_msg_tag(0, 0, 0, 0))
{
  const_cast<Utcb*>(utcb)->error = e;
  return L4_msg_tag(tag, L4_msg_tag::Error);
}

/// Check if `rights` has at least all `need` rights.
inline bool
check_rights(Rights rights, Rights need, L4_msg_tag *tag = 0)
{
  need &= Rights::CS() | Rights::CW();
  if (EXPECT_TRUE((rights & need) == need))
    return true;

  if (tag)
    *tag = commit_result(-L4_err::EPerm);
  return false;
}

/**
 * Check typical RPC conditions, such as the correct message size, the correct
 * label, and sufficient rights.
 */
inline bool
check_basics(L4_msg_tag *tag, Rights rights, long label,
             Rights need = Rights(0))
{
  if (EXPECT_FALSE(tag->proto() != label))
    {
      *tag = commit_result(-L4_err::EBadproto);
      return false;
    }

  if (EXPECT_FALSE(!check_rights(rights, need, tag)))
    return false;

  if (EXPECT_FALSE(tag->words() < 1))
    {
      *tag = commit_result(-L4_err::EMsgtooshort);
      return false;
    }

  return true;
}

/**
 * Helper function to dereference a capability send message
 * item and check the correct object type.
 */
template<typename OBJ>
OBJ *deref_next(L4_msg_tag *tag, Utcb const *utcb,
                L4_snd_item_iter &snd_items, Space *space,
                Rights *rights)
{
  if (!snd_items.next() || snd_items.get()->b.is_void())
    {
      *tag = commit_result(-L4_err::EInval);
      return 0;
    }

  L4_fpage fp(snd_items.get()->d);
  if (EXPECT_FALSE(!fp.is_objpage()))
    {
      *tag = commit_error(utcb, L4_error::Overflow);
      return 0;
    }

  OBJ *o = cxx::dyn_cast<OBJ*>(space->lookup_local(fp.obj_index(), rights));
  if (EXPECT_FALSE(!o))
    {
      *tag = commit_result(-L4_err::EInval);
      return 0;
    }

  return o;
}

/**
 * Helper to dereference exactly the first send message item as capability
 * of type `OBJ`.
 */
template<typename OBJ>
OBJ *deref(L4_msg_tag *tag, Utcb const *utcb, Rights *rights)
{
  L4_snd_item_iter snd_items(utcb, tag->words());
  if (!tag->items())
    {
      *tag = commit_result(-L4_err::EInval);
      return 0;
    }
  Space *const space = ::current()->space();
  if (!space)
    __builtin_unreachable();
  return deref_next<OBJ>(tag, utcb, snd_items, space, rights);
}

/// Helper to calculate the number of message words for given `bytes`.
inline constexpr unsigned long message_words(unsigned long bytes)
{ return (bytes + sizeof(Mword) - 1) / sizeof(Mword); }

/// Helper to check if `tag.words()` contains at least `size` bytes.
inline bool check_message_size(L4_msg_tag tag, unsigned long size)
{
  if (tag.words() < message_words(size))
    return false;
  if ((tag.words() + tag.items()) > Utcb::Max_words)
    return false;
  return true;
}

/// A typed capability input parameter for RPC
template<typename OBJ> struct Cap
{
  typedef OBJ Obj_type; ///< Type of the referenced kernel object
  Obj_type *obj;        ///< Pointer to the kernel object
  Rights rights;        ///< Rights bits from the capability (only CS | CW)
};

namespace Detail {

/// Descriptor for an RPC input data argument
template<typename T> struct Msg_item;

/// disallow reference arguments by leaving this type incomplete
template<typename T> struct Msg_item<T&>;

/// Descriptor for an RPC input data argument
template<typename T> struct Msg_item
{
  typedef T Arg_type;
  enum
  {
    in_size   = sizeof(T),
    in_align  = alignof(T),
    out_size  = 0,
    out_align = 1,
    in_items  = 0,
    out_items = 0,
  };

  static bool read_items(Arg_type *, L4_msg_tag *, Utcb const *,
                         L4_snd_item_iter &, Space *)
  { return true; }

  static void read_data(Arg_type *arg, char const *in, char *)
  { *arg = *reinterpret_cast<Arg_type const *>(in); }
};

/// Descriptor for an RPC input capability argument
template<typename T> struct Msg_item<Cap<T> >
{
  typedef Cap<T> Arg_type;

  enum
  {
    in_size   = 0,
    in_align  = 1,
    out_size  = 0,
    out_align = 1,
    in_items  = 1,
    out_items = 0,
  };

  static bool read_items(Arg_type *arg, L4_msg_tag *tag, Utcb const *in,
                         L4_snd_item_iter &snd_items, Space *s)
  {
    arg->obj = deref_next<T>(tag, in, snd_items, s, &arg->rights);
    return arg->obj != 0;
  }

  static void read_data(Arg_type *, char const *, char *)
  {}
};

/// Descriptor for an RPC output data argument
template<typename T> struct Msg_item<T*>
{
  typedef T *Arg_type;
  enum
  {
    in_size   = 0,
    in_align  = 1,
    out_size  = sizeof(T),
    out_align = alignof(T),
    in_items  = 0,
    out_items = 0,
  };

  static bool read_items(Arg_type *, L4_msg_tag *, Utcb const *,
                         L4_snd_item_iter &, Space *)
  { return true; }

  static void read_data(Arg_type *arg, char const *, char *out)
  { *arg = reinterpret_cast<Arg_type>(out); }
};

/// Align `pos` to `align` bytes use padding if needed
constexpr unsigned align(unsigned pos, unsigned align)
{ return (pos + align - 1) & ~(align - 1); }

/// Add an `ITEM` to the message described by `MSG`
template<typename MSG, typename ITEM>
struct Add_item
{
  enum
  {
    in_pos    = align(MSG::in_size, ITEM::in_align),
    in_size   = in_pos + ITEM::in_size,
    out_pos   = align(MSG::out_size, ITEM::out_align),
    out_size  = out_pos + ITEM::out_size,
    in_items  = MSG::in_items + ITEM::in_items,
    out_items = MSG::out_items + ITEM::out_items,
  };
};


/// Basic recursive RPC message arguments structure
template<typename STATE, typename ...ARGS>
struct R_msg;

template<typename STATE>
struct R_msg<STATE>
{
  // define this element as a terminator
  typedef void Tail;

  // assign the total sizes in the terminating
  // element in the recursive message structure
  enum
  {
    in_total_size   = STATE::in_size,
    out_total_size  = STATE::out_size,
    in_total_items  = STATE::in_items,
    out_total_items = STATE::out_items
  };

  bool read_items(L4_msg_tag *, Utcb const *,
                  L4_snd_item_iter &, Space *)
  { return true; }

  void read_data(char const *, char *) {}
};

template<typename STATE, typename A1, typename ...ARGS>
struct R_msg<STATE, A1, ARGS...> :
  R_msg<Add_item<STATE, Msg_item<A1> >, ARGS...>
{
  typedef Msg_item<A1> Item_info;
  typedef R_msg<Add_item<STATE, Item_info>, ARGS...> Tail;
  typedef typename Item_info::Arg_type Arg_type;
  typedef Add_item<STATE, Msg_item<A1> > Pos;

  Arg_type arg;

  bool read_items(L4_msg_tag *tag, Utcb const *in,
                  L4_snd_item_iter &snd_items, Space *s)
  {
    if (!Item_info::read_items(&arg, tag, in, snd_items, s))
      return false;
    return Tail::read_items(tag, in, snd_items, s);
  }

  void read_data(char const *in, char *out)
  {
    Item_info::read_data(&arg, in + Pos::in_pos, out + Pos::out_pos);
    Tail::read_data(in, out);
  }
};

/// allow per index access to an element in the argument structure
template<typename M, unsigned index>
struct Read_msg : Read_msg<typename M::Tail, index - 1> {};

template<typename M>
struct Read_msg<M, 0>
{
  typedef M Msg;
  typedef typename Msg::Arg_type Arg_type;
  static Arg_type get(M const *m)
  { return m->arg; }
};

/// Call a functor F with all arguments in the message structure `M`
template<typename M>
struct Call : Call<typename M::Tail>
{
  typedef Call<typename M::Tail> Base;
  template<typename F, typename ...ARGS> static
  L4_msg_tag call(M const &m, F &&func, ARGS &&...args)
  { return Base::call(m, func, cxx::forward<ARGS>(args)..., m.arg); }
};

template<typename S>
struct Call<R_msg<S> >
{
  template<typename MSG, typename F, typename ...ARGS> static
  L4_msg_tag call(MSG const &, F &&func, ARGS &&...args)
  { return func(cxx::forward<ARGS>(args)...); }
};


/// Start state for a message structure (skipping the opcode)
struct Msg_start_state
{
  enum
  {
    in_size = sizeof(Mword), // skip the opcode word in the input message
    out_size = 0,
    in_items = 0,
    out_items = 0
  };
};

} // namespace Detail

/**
 * A kernel-side RPC message description for a single method call.
 *
 * This type describes a method call with arguments given by `ARGS`.
 * Currently this code supports fixed-size static messages with input and
 * output data parameters, and input capability parameters.
 * NOTE: output capabilities are not supported because they are used by the
 * Factory only.
 *
 * Data value arguments are treated as input data. Pointer arguments are
 * treated as output arguments and `Ko::Cap<>` arguments are input
 * capabilities.  References are not allowed as arguments.
 *
 * An instance of Msg<> provides storage for all input arguments, including
 * resolved capabilities with rights, and for pointers to the output arguments
 * stored in the output-UTCB.
 */
template<typename ...ARGS>
struct Msg : Detail::R_msg<Detail::Msg_start_state, ARGS...>
{
  /// The base class type representing the message itself
  typedef Detail::R_msg<Detail::Msg_start_state, ARGS...> Base;
  /// The type itself
  typedef Msg<ARGS...> Self;

  /**
   * check if the message described by tag contains enough input data for this
   * message.
   *
   * NOTE: this does not include checks for the input capabilities, these are
   * done during `read()`.
   */
  static bool check_size(L4_msg_tag tag)
  { return check_message_size(tag, Base::in_total_size); }

  /**
   * Read the input arguments and set the pointers for the output arguments to
   * the output-UTCB.
   *
   * The function includes the appropriate size checks.
   */
  bool read(L4_msg_tag *tag, Utcb const *in, Utcb *out)
  {
    if (!check_size(*tag) || (Base::in_total_items && !tag->items()))
      {
        // message too short
        *tag = commit_result(-L4_err::EMsgtooshort);
        return false;
      }

    if (Base::in_total_items)
      {
        L4_snd_item_iter snd_items(in, tag->words());
        Space *s = ::current()->space();

        if (!Base::read_items(tag, in, snd_items, s))
          return false;
      }

    Base::read_data((char const *)(in->values), (char *)out->values);
    __asm__ __volatile__ (
        ""
        : "=m" (*const_cast<Mword (*)[Utcb::Max_words]>(&in->values))
    );
    return true;
  }

  /// Access arguments using their position
  template<unsigned arg>
  typename Detail::Read_msg<Base, arg>::Arg_type get()
  { return Detail::Read_msg<Base, arg>::get(this); }

  /**
   * Parse the input message and call the given function with the parsed
   * arguments.
   *
   * This function is to be used in the dispatch demultiplexing code.
   *
   * Extra arguments passed to this function are passed to `func` before the
   * arguments of the message.
   *
   * After a successful call to `func` the output message tag is updated to the
   * correct number of words and items.
   */
  template<typename F, typename ...EXTRA> static
  L4_msg_tag call(L4_msg_tag tag, Utcb const *in, Utcb *out, F &&func,
                  EXTRA &&...args)
  {
    Self msg;
    if (!msg.read(&tag, in, out))
      return tag;

    L4_msg_tag t
      = Detail::Call<Msg>::call(msg, func, cxx::forward<EXTRA>(args)...);

    // !do_switch means do not send an answer...
    if (EXPECT_FALSE(t.has_error() || !t.do_switch()))
      return t;

    if (EXPECT_FALSE(t.proto() < 0))
      return t;

    return L4_msg_tag(message_words(msg.out_total_size), msg.out_total_items,
                      t.flags(), t.proto());
  }
};

/// RPC Message for the function arguments of the given signature type
template<typename SIG> struct Sig_msg;
template<typename ...ARGS> struct Sig_msg<L4_msg_tag (ARGS...)> : Msg<ARGS...> {};

/**
 * Define a message class `Msg_##name` for the given arguments.
 */
#define L4_RPC(opcode, name, fargs...)                 \
  struct Msg_##name : Ko::Sig_msg<L4_msg_tag fargs>    \
  {                                                    \
    enum : Mword { Op = opcode };                      \
    template<typename OBJ> struct Fwd                  \
    {                                                  \
      OBJ *o;                                          \
      constexpr Fwd(OBJ *o) : o(o) {}                  \
      template<typename ...ARGS>                       \
      L4_msg_tag operator () (ARGS ...a) const         \
      { return o->op_##name(a...); }                   \
    };                                                 \
    template<typename OBJ, typename ...ARGS>           \
    static L4_msg_tag call(OBJ *o, L4_msg_tag tag,     \
                           Utcb const *in, Utcb *out,  \
                           ARGS &&...args)             \
    {                                                  \
      typedef Ko::Sig_msg<L4_msg_tag fargs> Self;      \
      return  Self::call(tag, in, out, Fwd<OBJ>(o),    \
                         cxx::forward<ARGS>(args)...); \
    }                                                  \
  };
}
