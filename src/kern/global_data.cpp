// --------------------------------------------------------------------------
INTERFACE[!amp]:

#define DEFINE_GLOBAL_PRIO(prio) __attribute__((init_priority(prio)))
#define DEFINE_GLOBAL

template< typename T >
class Global_data_ptr_storage
{
public:
  constexpr Global_data_ptr_storage() = default;

  constexpr explicit Global_data_ptr_storage(T *ptr) : _d(ptr) {}

protected:
  constexpr T *&_get() { return _d; }
  constexpr T * const &_get() const { return _d; }

private:
  T *_d;
};

template< typename T >
class Global_data_obj_storage
{
public:
  constexpr Global_data_obj_storage() = default;

  template<typename... A>
  constexpr explicit Global_data_obj_storage(A&&... args)
  : _d(cxx::forward<A>(args)...)
  {}

protected:
  constexpr T *_get() { return &_d; }
  constexpr T const *_get() const { return &_d; }

private:
  T _d;
};

// --------------------------------------------------------------------------
INTERFACE[amp]:

#define DEFINE_GLOBAL_PRIO(prio) \
  __attribute__((section(".global_data"), init_priority(prio)))

#define DEFINE_GLOBAL \
  __attribute__((section(".global_data")))

/**
 * Architecture specific base class for global data.
 */
class Global_data_base
{
protected:
  /**
   * Apply offset of current node to pointer.
   *
   * An architecure that supports AMP must implement this method.
   *
   * \param  node0_obj Pointer to primary node object
   * \return Pointer to current node object
   */
  static inline char *local_addr(char *node0_obj);
};

/// This AMP implementation will access the node local storage for the pointer.
template< typename T >
class Global_data_ptr_storage : public Global_data_base
{
public:
  Global_data_ptr_storage() = default;

  explicit Global_data_ptr_storage(T *ptr) { _get() = ptr; }

protected:
  T *&_get() const
  { return *reinterpret_cast<T**>(local_addr(reinterpret_cast<char*>(&_i))); }

private:
  mutable T *_i;
};

/// This AMP implementation will access the node local storage for the object.
template< typename T >
class Global_data_obj_storage : public Global_data_base
{
public:
  Global_data_obj_storage()
  { _construct(this); }

  template<typename... A>
  explicit Global_data_obj_storage(A&&... args)
  { _construct(this, cxx::forward<A>(args)...); }

protected:
  T *_get() const
  { return reinterpret_cast<T*>(local_addr(&_i[0])); }

private:
  static void _construct(Global_data_obj_storage *self)
  { new (local_addr(self->_i)) T; }

  template< typename... A >
  static void _construct(Global_data_obj_storage *self, A&&... args)
  { new (local_addr(self->_i)) T(cxx::forward<A>(args)...); }

  mutable char __attribute__((aligned(alignof(T)))) _i[sizeof(T)];
};

// --------------------------------------------------------------------------
INTERFACE:

#include "types.h"

/**
 * Wrapper for global objects.
 *
 * Will implicitly convert to the underlying type. Tries to be as transparent as
 * possible.
 */
template< typename T >
class Global_data final : private Global_data_obj_storage<T>
{
public:
  using Global_data_obj_storage<T>::Global_data_obj_storage;

  Global_data(Global_data const &) = delete;
  Global_data &operator = (Global_data const &) = delete;

  void operator=(T const &o) { *_get() = o; }

  constexpr T &unwrap() { return *_get(); }
  constexpr T const &unwrap() const { return *_get(); }

  constexpr operator T&() { return *_get(); }
  constexpr operator T const &() const { return *_get(); }

  constexpr T *operator&() { return _get(); }
  constexpr T const *operator&() const { return _get(); }

  constexpr T *operator->() { return _get(); }
  constexpr T const *operator->() const { return _get(); }

  template<typename U = T, typename = decltype(U().begin())>
  constexpr auto begin() { return _get()->begin(); }

  template<typename U = T, typename = decltype(U().end())>
  constexpr auto end() { return _get()->end(); }

private:
  using Global_data_obj_storage<T>::_get;
};

/**
 * Global data wrapper specialization for pointer types.
 *
 * Tries to be as transparent as possible for global pointers.
 */
template< typename T >
class Global_data<T *> final : private Global_data_ptr_storage<T>
{
public:
  using Global_data_ptr_storage<T>::Global_data_ptr_storage;

  Global_data(Global_data const &) = delete;
  Global_data &operator = (Global_data const &) = delete;

  void operator=(T *ptr) { _get() = ptr; }

  constexpr T **operator&() { return &_get(); }
  constexpr T * const *operator&() const { return &_get(); }

  constexpr operator T * () const { return _get(); }
  constexpr T *operator -> () const { return _get(); }

  constexpr T *&unwrap() { return _get(); }
  constexpr T * const &unwrap() const { return _get(); }

private:
  using Global_data_ptr_storage<T>::_get;
};

/**
 * Global data wrapper specialization for static objects.
 *
 * Tries to be as transparent as possible for global static objects.
 */
template< typename T >
class Global_data<Static_object<T>> final
: private Global_data_obj_storage<Static_object<T>>
{
public:
  constexpr Global_data() = default;

  Global_data(Global_data const &) = delete;
  Global_data &operator = (Global_data const &) = delete;

  constexpr Static_object<T> &unwrap() { return *_get(); }
  constexpr Static_object<T> const &unwrap() const { return *_get(); }

  constexpr operator T * () const { return get(); }
  constexpr T *operator -> () const { return get(); }

  T *construct() { return _get()->construct(); }

  template< typename... A >
  T *construct(A&&... args)
  { return _get()->construct(cxx::forward<A>(args)...); }

  constexpr T *get() const { return _get()->get(); }
  constexpr T *get_unchecked() { return _get()->get_unchecked(); }

private:
  using Global_data_obj_storage<Static_object<T>>::_get;
};
