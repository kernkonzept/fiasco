INTERFACE[amp]:

#include "types.h"

class Per_node_data_base
{
protected:
  static inline char *local_addr(char *node0_obj);
};

template< typename T >
class Per_node_data final : public Per_node_data_base
{
public:
  Per_node_data(Per_node_data const &) = delete;
  Per_node_data &operator = (Per_node_data const &) = delete;

  Per_node_data()
  {
    construct(this);
  }

  template<typename... A>
  Per_node_data(A&&... args)
  {
    construct(this, cxx::forward<A>(args)...);
  }

  T *get() const
  {
    return reinterpret_cast<T*>(local_addr(&_i[0]));
  }

  operator T * () const { return get(); }
  T *operator -> () const { return get(); }

  operator bool() const = delete;

private:
  static void construct(Per_node_data *self)
  { new (local_addr(self->_i)) T; }

  template< typename... A >
  static void construct(Per_node_data *self, A&&... args)
  { new (local_addr(self->_i)) T(cxx::forward<A>(args)...); }

  mutable char __attribute__((aligned(alignof(T)))) _i[sizeof(T)];
};

#define DECLARE_PER_NODE_PRIO(prio) \
  __attribute__((section(".per_node.data"), init_priority(prio)))

#define DECLARE_PER_NODE \
  __attribute__((section(".per_node.data")))

// --------------------------------------------------------------------------
INTERFACE[!amp]:

#include "types.h"

class Per_node_data_base
{
};

template< typename T >
class Per_node_data final : public Per_node_data_base
{
public:
  Per_node_data(Per_node_data const &) = delete;
  Per_node_data &operator = (Per_node_data const &) = delete;

  Per_node_data() = default;

  template<typename... A>
  Per_node_data(A&&... args)
  : _d(cxx::forward<A>(args)...)
  {}

  T *get() const
  {
    return &_d;
  }

  operator T * () const { return get(); }
  T *operator -> () const { return get(); }

  operator bool() const = delete;

private:
  mutable T _d;
};

#define DECLARE_PER_NODE_PRIO(prio) __attribute__((init_priority(prio)))
#define DECLARE_PER_NODE

// --------------------------------------------------------------------------
IMPLEMENTATION:

