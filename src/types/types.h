#ifndef TYPES_H__
#define TYPES_H__

#include <stddef.h>
#include "types-arch.h"
#include "std_macros.h"

#ifdef __cplusplus

#include <cassert>
#include <cstdint>
#include <cxx/cxx_int>
#include <cxx/type_traits>
#include <new>

template< typename T > inline
T expect(T v, T expected)
{
  return static_cast<T>(__builtin_expect(static_cast<long>(v), static_cast<long>(expected)));
}

template< typename a, typename b > inline
a nonull_static_cast( b p )
{
  if (!p)
    __builtin_unreachable();
  return static_cast<a>(p);
}

/**
 * Cast an object to a different type with an offset.
 *
 * \tparam T1      Target object type.
 * \tparam T2      Source object type.
 * \param  offset  Byte offset within the source object.
 *
 * \return Target object.
 */
template<typename T1, typename T2> inline
T1 offset_cast(T2 ptr, uintptr_t offset)
{
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr) + offset;
  return reinterpret_cast<T1>(addr);
}

/**
 * Read the value at an address at most once.
 *
 * The read might be omitted if the result is not used by any code unless
 * `typename` contains `volatile`. If the read operation has side effects and
 * must not be omitted, use different means like #Mmio_register_block or
 * similar.
 *
 * The compiler is disallowed to reuse a previous read at the same address, for
 * example:
 * ```
 * val1 = *a;
 * val2 = access_once(a);  // compiler may not replace this by val2 = val1;
 * ```
 *
 * The compiler is also disallowed to repeat the read, for example:
 * ```
 * val1 = access_once(a);
 * val2 = val1;  // compiler may not replace this by val2 = *a;
 * ```
 *
 * The above implies that the compiler is also disallowed to move the read out
 * of or into loops.
 *
 * \note The read might still be moved relative to other code.
 * \note The value might be read from a hardware cache, not from RAM.
 */
template< typename T > ALWAYS_INLINE inline
T access_once(T const *a)
{
#if 1
  __asm__ __volatile__ ( "" : "=m"(*const_cast<T*>(a)));
  T tmp = *a;
  __asm__ __volatile__ ( "" : "=m"(*const_cast<T*>(a)));
  return tmp;
#else
  return *static_cast<T const volatile *>(a);
#endif
}

/**
 * Write a value at an address exactly once.
 *
 * The compiler is disallowed to skip the write, for example:
 * ```
 * *a = val;
 * write_now(a, val);  // compiler may not skip this line
 * ```
 *
 * The compiler is also disallowed to repeat the write.
 *
 * The above implies that the compiler is also disallowed to move the write out
 * of or into loops.
 *
 * \note The write might still be moved relative to other code.
 * \note The value might be written just to a hardware cache for the moment, not
 *       immediately to RAM.
 */
template< typename T, typename VAL > ALWAYS_INLINE inline
void write_now(T *a, VAL &&val)
{
  __asm__ __volatile__ ( "" : "=m"(*a));
  *a = val;
  __asm__ __volatile__ ( "" : : "m"(*a));
}

template< typename T >
class Static_object
{
public:
  // prohibit copies
  Static_object(Static_object const &) = delete;
  Static_object &operator = (Static_object const &) = delete;
  Static_object() = default;

  /**
   * Get the pointer to \ref _i.
   *
   * Asserts that the \ref construct() method has been called before.
   *
   * \return Pointer to \ref _i.
   */
  T *get() const
  {
    assert(_constructed);
    return get_unchecked();
  }

  /**
   * Get the pointer to \ref _i.
   *
   * This method should only be called in specific circumstances where we
   * explicitly do not want to call \ref assert() (potentially to avoid
   * infinite recursion).
   *
   * \return Pointer to \ref _i.
   */
  T *get_unchecked() const
  {
    return reinterpret_cast<T*>(&_i[0]);
  }

  operator T * () const { return get(); }
  T *operator -> () const { return get(); }

  T *construct()
  {
    assert(!_constructed);
    _constructed = true;

    return new (_i) T;
  }

  template< typename... A >
  T *construct(A&&... args)
  {
    assert(!_constructed);
    _constructed = true;

    return new (_i) T(cxx::forward<A>(args)...);
  }

private:
  mutable char __attribute__((aligned(sizeof(Mword)*2))) _i[sizeof(T)];

  /**
   * Internal metadata indicating whether the \ref construct() method has been
   * already called.
   *
   * Accessing \ref _i prior to construction might constitute undefined
   * behavior.
   */
  mutable bool _constructed;
};

typedef cxx::int_type<unsigned, struct Order_t> Order;
typedef cxx::int_type_order<unsigned long, struct Bytes_t, Order> Bytes;

namespace Addr {

template< int SHIFT >
struct Order : cxx::int_base<unsigned, Order<SHIFT> >, cxx::int_diff_ops<Order<SHIFT> >
{
  typedef cxx::int_base<unsigned, Order<SHIFT> > Base_class;
  Order() = default;
  explicit constexpr Order(typename Base_class::Value v) : Base_class(v) {}
};


template< int SHIFT, typename T >
class Addr_val
: public cxx::int_base< Address, T >,
  public cxx::int_null_chk<T>,
  public cxx::int_shift_ops<T, Order<SHIFT> >
{
private:
  typedef cxx::int_base< Address, T > B;

  static constexpr Address __shift(Address x, int oshift)
  { return ((SHIFT - oshift) >= 0) ? (x << (SHIFT - oshift)) : (x >> (oshift - SHIFT)); }

public:
  enum { Shift = SHIFT };

  template< int OSHIFT >
  constexpr Addr_val(Addr_val<OSHIFT, T> o)
  : B(__shift(Addr_val<OSHIFT, T>::val(o), OSHIFT))
  {}

  explicit constexpr Addr_val(Address a) : B(a) {}

  //explicit Addr_val(::Order o) : B(Address{1} << (Order::val(o) - ARCH_PAGE_SHIFT + SHIFT)) {}

  constexpr Addr_val(Addr_val const volatile &o) : B(o._v) {}
  Addr_val(Addr_val const &)  = default;
  Addr_val() = default;
  Addr_val &operator = (Addr_val const &o) = default;

  template< int OSHIFT >
  constexpr T operator << (Order<OSHIFT> const &o) const
  { return T(this->_v << (SHIFT + Order<OSHIFT>::val(o) - OSHIFT)); }

protected:
  constexpr Addr_val(typename B::Value v, int oshift) : B(__shift(v, oshift)) {}
};

template< int SHIFT >
struct Diff
: public Addr_val< SHIFT, Diff<SHIFT> >,
  cxx::int_diff_ops<Diff<SHIFT> >,
  cxx::int_bit_ops<Diff<SHIFT> >
{
  typedef Addr_val< SHIFT, Diff<SHIFT> > Base_class;
  Diff() = default;
  Diff(Diff const &) = default;
  explicit constexpr Diff(Address a) : Base_class(a) {}
  //Diff(::Order o) : Base_class(o) {}
  Diff &operator= (Diff const &) = default;

  template< int OSHIFT >
  constexpr Diff(Diff<OSHIFT> o) : Base_class(Diff<OSHIFT>::val(o), OSHIFT) {}
};

template< int SHIFT >
struct Addr
: public Addr_val< SHIFT, Addr<SHIFT> >,
  cxx::int_diff_ops<Addr<SHIFT>, Diff<SHIFT> >,
  cxx::int_bit_ops<Addr<SHIFT>, Diff<SHIFT> >
{
  typedef Addr_val< SHIFT, Addr<SHIFT> > Base_class;
  Addr() = default;
  Addr(Addr const &) = default;
  explicit constexpr Addr(Address a) : Base_class(a) {}
  Addr &operator= (Addr const &) = default;

  template< int OSHIFT >
  constexpr Addr(Addr<OSHIFT> o) : Base_class(Addr<OSHIFT>::val(o), OSHIFT) {}
};

} // namespace Addr

class Virt_addr
: public Addr::Addr<ARCH_PAGE_SHIFT>
{
public:
  template< int OSHIFT >
  constexpr Virt_addr(typename ::Addr::Addr<OSHIFT> const &o) : ::Addr::Addr<ARCH_PAGE_SHIFT>(o) {}

  template< int OSHIFT >
  constexpr Virt_addr(typename ::Addr::Addr<OSHIFT>::Type const &o) : ::Addr::Addr<ARCH_PAGE_SHIFT>(o) {}

  explicit constexpr Virt_addr(Address a) : ::Addr::Addr<ARCH_PAGE_SHIFT>(a) {}
  explicit constexpr Virt_addr(int a) : ::Addr::Addr<ARCH_PAGE_SHIFT>(a) {}
  explicit constexpr Virt_addr(unsigned a) : ::Addr::Addr<ARCH_PAGE_SHIFT>(a) {}
  explicit constexpr Virt_addr(long a) : ::Addr::Addr<ARCH_PAGE_SHIFT>(a) {}

  Virt_addr(void *a) : ::Addr::Addr<ARCH_PAGE_SHIFT>(reinterpret_cast<Address>(a)) {}

  explicit operator void * () const
  { return reinterpret_cast<void *>(_v); }

  Virt_addr() = default;
};

typedef Addr::Diff<ARCH_PAGE_SHIFT> Virt_size;
typedef Addr::Order<ARCH_PAGE_SHIFT> Virt_order;

typedef Addr::Addr<0> Page_number;
typedef Addr::Diff<0> Page_count;
//typedef Addr::Order<0> Page_order;
//
typedef Addr::Addr<ARCH_PAGE_SHIFT> Phys_mem_addr;
typedef Addr::Diff<ARCH_PAGE_SHIFT> Phys_mem_size;

template<typename T>
struct Simple_ptr_policy
{
  typedef T &Deref_type;
  typedef T *Ptr_type;
  typedef T *Member_type;
  typedef T *Storage_type;

  static void init(Storage_type &d) { d = nullptr; }
  static void init(Storage_type &d, Storage_type const &s) { d = s; }
  static void copy(Storage_type &d, Storage_type const &s) { d = s; }
  static void destroy(Storage_type const &) {}
  static constexpr Deref_type deref(Storage_type p) { return *p; }
  static constexpr Member_type member(Storage_type p) { return p; }
  static constexpr Ptr_type ptr(Storage_type p) { return p; }
};

template<>
struct Simple_ptr_policy<void>
{
  typedef void Deref_type;
  typedef void *Ptr_type;
  typedef void Member_type;
  typedef void *Storage_type;

  static void init(Storage_type &d) { d = nullptr; }
  static void init(Storage_type &d, Storage_type const &s) { d = s; }
  static void copy(Storage_type &d, Storage_type const &s) { d = s; }
  static void destroy(Storage_type const &) {}
  static Deref_type deref(Storage_type p);
  static Member_type member(Storage_type p);
  static constexpr Ptr_type ptr(Storage_type p) { return p; }
};


template<typename T, template<typename P> class Policy = Simple_ptr_policy,
         typename Discriminator = int>
class Smart_ptr
{
public:
  typedef typename Policy<T>::Deref_type Deref_type;
  typedef typename Policy<T>::Ptr_type Ptr_type;
  typedef typename Policy<T>::Member_type Member_type;
  typedef typename Policy<T>::Storage_type Storage_type;

  template<typename A, template<typename X> class B, typename C>
  friend class Smart_ptr;

protected:
  Storage_type _p;

public:
  constexpr Smart_ptr()
  { Policy<T>::init(_p); }

  explicit constexpr Smart_ptr(T *p)
  { Policy<T>::init(_p, p); }

  constexpr Smart_ptr(Smart_ptr const &o)
  { Policy<T>::copy(_p, o._p); }

  template< typename RHT >
  constexpr Smart_ptr(Smart_ptr<RHT, Policy, Discriminator> const &o)
  { Policy<T>::copy(_p, o._p); }

  ~Smart_ptr()
  { Policy<T>::destroy(_p); }

  Smart_ptr operator = (Smart_ptr const &o)
  {
    if (this == &o)
      return *this;

    Policy<T>::destroy(_p);
    Policy<T>::copy(_p, o._p);
    return *this;
  }

  constexpr Deref_type operator * () const
  { return Policy<T>::deref(_p); }

  constexpr Member_type operator -> () const
  { return Policy<T>::member(_p); }

  constexpr Ptr_type get() const
  { return Policy<T>::ptr(_p); }

  explicit constexpr operator bool () const
  { return Policy<T>::ptr(_p) != nullptr; }
};

enum User_ptr_discr {};

template<typename T>
using User_ptr = Smart_ptr<T, Simple_ptr_policy, User_ptr_discr>;

struct Cpu_number : cxx::int_type_order_base<unsigned, Cpu_number, Order>
{
  Cpu_number() = default;
  explicit constexpr Cpu_number(unsigned n) : cxx::int_type_order_base<unsigned, Cpu_number, Order>(n) {}

  static constexpr Cpu_number boot_cpu() { return Cpu_number(0); }
  static constexpr Cpu_number first()    { return Cpu_number(0); }
  static constexpr Cpu_number second()   { return Cpu_number(1); }
  static constexpr Cpu_number nil()      { return Cpu_number(~0); }
};


/**
 * Write a hardware cached value, such as a page-table entry, into memory.
 *
 * The function provides safe update of hardware cached values in memory that
 * have a valid / present encoding that must be kept consistent and a not
 * present encoding that is ignored by hardware.
 *
 * In the case of multi-word values the implementation assumes the present /
 * valid bits to be in the first word of the value and if the value to be
 * written returns true for v.present() the first word in the target will be
 * first written to '0' (assuming this means invalid / ignored by hardware).
 * Then the uppermost words will be updated and after that the first word will
 * be set to the value given in v.
 *
 * NOTE: in cases where the size of T is smaller or equal to the size of an Mword
 * The update is a single write_now(...).
 */
template<typename T>
inline void
write_consistent(cxx::enable_if_t<! (sizeof(T) > sizeof(Mword)), T> *t,
                 T const &v)
{ write_now(t, v); }

template<typename T>
inline void
write_consistent(cxx::enable_if_t<(sizeof(T) > sizeof(Mword)), T> *t,
                 T const &v)
{
  static_assert ((sizeof(T) % sizeof(Mword)) == 0, "type must be a multiple of Mwords");
  // number of words for type T
  enum { Words = sizeof(T) / sizeof(Mword) };
  // array for the words of T
  typedef Mword Words_array[Words];
  // union for aliasing
  union X { Words_array w;  T t; };

  // destination
  Mword *d = reinterpret_cast<X *>(t)->w;
  // source
  Mword const *s = reinterpret_cast<X const &>(v).w;

  __asm__ __volatile__ ( "" : "=m"(d[0]));
  if (v.present())
    d[0] = 0;
  else
    d[0] = s[0];
  __asm__ __volatile__ ( "" : : "m"(d[0]));

  __asm__ __volatile__ ( "" : "=m"(reinterpret_cast<Words_array&>(*d)));
  for (unsigned i = 1; i < Words; ++i)
    d[i] = s[i];
  __asm__ __volatile__ ( "" : : "m"(reinterpret_cast<Words_array&>(*d)));

  if (v.present())
    {
      __asm__ __volatile__ ( "" : "=m"(d[0]));
      d[0] = s[0];
      __asm__ __volatile__ ( "" : : "m"(d[0]));
    }
}

namespace cxx {

/** Return the number of elements of a fixed-sized C array. */
template<typename T, unsigned N>
constexpr unsigned
size(const T(&)[N]) noexcept
{ return N; }

}

#endif

/// standard size type
///typedef mword_t size_t;
///typedef signed int ssize_t;

/// momentary only used in UX since there the kernel has a different
/// address space than user mode applications
enum Address_type { ADDR_KERNEL = 0, ADDR_USER = 1, ADDR_UNKNOWN = 2 };

#endif // TYPES_H__
