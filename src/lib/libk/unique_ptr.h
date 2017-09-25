#pragma once

#include <cxx/type_traits>

namespace cxx
{

template< typename T >
struct default_delete
{
  default_delete() {}

  template< typename U >
  default_delete(default_delete<U> const &) {}

  void operator () (T *p) const
  { delete p; }
};

template< typename T >
struct default_delete<T[]>
{
  default_delete() {}

  void operator () (T *p)
  { delete [] p; }
};

template< typename T, typename T_Del = default_delete<T> >
class unique_ptr
{
private:
  struct _unspec;
  typedef _unspec* _unspec_ptr_type;

public:
  typedef T *pointer;
  typedef T element_type;
  typedef T_Del deleter_type;

  unique_ptr() : _ptr(pointer()) {}

  explicit unique_ptr(pointer p) : _ptr(p) {}

  unique_ptr(unique_ptr &&o) : _ptr(o.release()) {}

  ~unique_ptr() { reset(); }

  unique_ptr &operator = (unique_ptr &&o)
  {
    reset(o.release());
    return *this;
  }

  unique_ptr &operator = (_unspec_ptr_type)
  {
    reset();
    return *this;
  }

  element_type &operator * () const { return *get(); }
  pointer operator -> () const { return get(); }

  pointer get() const { return _ptr; }

  operator _unspec_ptr_type () const
  { return reinterpret_cast<_unspec_ptr_type>(get()); }

  pointer release()
  {
    pointer r = _ptr;
    _ptr = 0;
    return r;
  }

  void reset(pointer p = pointer())
  {
    if (p != get())
      {
        deleter_type()(get());
        _ptr = p;
      }
  }

  unique_ptr(unique_ptr const &) = delete;
  unique_ptr &operator = (unique_ptr const &) = delete;

private:
  pointer _ptr;
};

}
