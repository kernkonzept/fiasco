/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */

#pragma once

namespace cxx { namespace Bits {

template< typename T >
class List_iterator_end_ptr
{
private:
  template< typename U > friend class Basic_list;
  static void *_end;
};

template< typename T >
void *List_iterator_end_ptr<T>::_end;

template< typename VALUE_T, typename TYPE >
struct Basic_list_policy
{
  typedef VALUE_T *Value_type;
  typedef VALUE_T const *Const_value_type;
  typedef TYPE **Type;
  typedef TYPE *Const_type;
  typedef TYPE *Head_type;

  static Type next(Type c) { return &(*c)->_n; }
  static Const_type next(Const_type c) { return c->_n; }
};

template< typename POLICY >
class Basic_list
{
  Basic_list(Basic_list const &) = delete;
  void operator = (Basic_list const &) = delete;

public:
  typedef typename POLICY::Value_type Value_type;
  typedef typename POLICY::Const_value_type Const_value_type;

  class End_iterator {};

  class Iterator
  {
  private:
    typedef typename POLICY::Type Internal_type;

  public:
    typedef typename POLICY::Value_type value_type;
    typedef typename POLICY::Value_type Value_type;

    Value_type operator * () const { return static_cast<Value_type>(*_c); }
    Value_type operator -> () const { return static_cast<Value_type>(*_c); }
    Iterator operator ++ () { _c = POLICY::next(_c); return *this; }

    bool operator == (End_iterator const &) const { return *_c == 0; }
    bool operator != (End_iterator const &) const { return *_c != 0; }
    bool operator == (Iterator const &o) const { return *_c == *o._c; }
    bool operator != (Iterator const &o) const { return !operator == (o); }

    Iterator() {}
    Iterator(End_iterator const &) : _c(__end()) {}

  private:
    friend class Basic_list;
    static Internal_type __end()
    {
      union X { Internal_type l; void **v; } z;
      z.v = &Bits::List_iterator_end_ptr<void>::_end;
      return z.l;
    }

    explicit Iterator(Internal_type i) : _c(i) {}

    Internal_type _c;
  };

  class Const_iterator
  {
  private:
    typedef typename POLICY::Const_type Internal_type;

  public:
    typedef typename POLICY::Value_type value_type;
    typedef typename POLICY::Value_type Value_type;

    Value_type operator * () const { return static_cast<Value_type>(_c); }
    Value_type operator -> () const { return static_cast<Value_type>(_c); }
    Const_iterator operator ++ () { _c = POLICY::next(_c); return *this; }

    bool operator == (End_iterator const &) const { return _c == 0; }
    bool operator != (End_iterator const &) const { return _c != 0; }
    bool operator == (Const_iterator const &o) const { return _c == o._c; }
    bool operator != (Const_iterator const &o) const { return !operator == (o); }

    Const_iterator() {}
    Const_iterator(End_iterator const &) : _c(0) {}
    Const_iterator(Iterator const &o) : _c(*o) {}

  private:
    friend class Basic_list;

    explicit Const_iterator(Internal_type i) : _c(i) {}

    Internal_type _c;
  };

  // BSS allocation
  explicit Basic_list(bool) {}
  Basic_list() : _f(0) {}

  Basic_list(Basic_list &&o) : _f(o._f)
  {
    o.clear();
  }

  Basic_list &operator = (Basic_list &&o)
  {
    _f = o._f;
    o.clear();
    return *this;
  }

  bool empty() const { return !_f; }
  Value_type front() const { return static_cast<Value_type>(_f); }

  void clear() { _f = 0; }

  Iterator begin() { return Iterator(&_f); }
  Const_iterator begin() const { return Const_iterator(_f); }
  static Const_iterator iter(Const_value_type c) { return Const_iterator(c); }
  static End_iterator end() { return End_iterator(); }

protected:
  static typename POLICY::Type __get_internal(Iterator const &i) { return i._c; }
  static Iterator __iter(typename POLICY::Type c) { return Iterator(c); }

  typename POLICY::Head_type _f;
};

}}

