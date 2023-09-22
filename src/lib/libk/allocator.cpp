INTERFACE:

#include <spin_lock.h>
#include <lock_guard.h>

/**
 * Allocator locking policy that implements no locking.
 *
 * Only use this allocation locking policy when the mutual exclusion of calling
 * the methods of an allocator instance is guaranteed externally (i.e. a stricly
 * local use in a single thread, an external lock being already held, etc.).
 */
class Lockless_policy
{
public:
  /**
   * Fake lock type that occupies no space.
   *
   * The lock member needs to be declared as the last member of a class.
   */
  using Lock_type = unsigned int[];

  /**
   * No-op lock guard factory.
   */
  static inline Lock_type &lock_guard(Lock_type &lock)
  {
    return lock;
  }
};

/**
 * Allocator locking policy using a spinlock.
 */
class Spinlock_policy
{
public:
  /**
   * Spinlock lock type.
   */
  using Lock_type = Spin_lock<>;

  /**
   * Lock guard factory.
   *
   * Zero-copy pass-by-value of the return value is guaranteed by copy elision
   * in C++17.
   */
  static inline Lock_guard<Lock_type> lock_guard(Lock_type &lock)
  {
    return ::lock_guard(lock);
  }
};

/**
 * Simple pointer-bumping allocator.
 *
 * A trivial allocator that allocates dynamic memory from a supplied memory
 * block by bumping a pointer. Deallocation is not supported.
 *
 * \tparam LOCKING_POLICY  Locking policy class that defines a Lock_type and
 *                         a lock_guard() static method.
 */
template<typename LOCKING_POLICY>
class Simple_alloc
{
public:
  Simple_alloc() = default;

  /**
   * Construct the allocator.
   *
   * \param block  Memory block from which to allocate.
   * \param size   Size of the memory block.
   */
  Simple_alloc(void *block, size_t size)
  : _ptr(reinterpret_cast<uintptr_t>(block)), _size(size)
  {}

  /**
   * Construct the allocator.
   *
   * \param block  Memory block address from which to allocate.
   * \param size   Size of the memory block.
   */
  Simple_alloc(uintptr_t block, size_t size)
  : _ptr(block), _size(size)
  {}

  /**
   * Allocate dynamic memory.
   *
   * \note The allocated object is not initialized/constructed.
   *
   * \tparam T  Type of the object to allocate.
   *
   * \param size       Size of the object to allocate.
   * \param alignment  Alignment of the object to allocate.
   *
   * \return Allocated object or nullptr on failure.
   */
  template<typename T>
  T *alloc_bytes(size_t size, size_t alignment = 1)
  {
    [[maybe_unused]] auto guard = LOCKING_POLICY::lock_guard(_lock);

    if (size > _size)
      return nullptr;

    alignment -= 1;

    uintptr_t ptr_aligned = (_ptr + alignment) & ~alignment;
    uintptr_t gap = ptr_aligned - _ptr;

    if (gap >= _size)
      return nullptr;

    if (gap + size > _size)
      return nullptr;

    _size -= gap + size;
    _ptr = ptr_aligned + size;

    return reinterpret_cast<T *>(ptr_aligned);
  }

  /**
   * Allocate dynamic memory.
   *
   * \note The allocated object is not initialized/constructed.
   *
   * \tparam T  Type of the objects to allocate.
   *
   * \param count      Number of the objects to allocate.
   * \param alignment  Alignment of the first object to allocate.
   *
   * \return Allocated objects or nullptr on failure.
   */
  template<typename T>
  T *alloc(size_t count = 1, size_t alignment = alignof(T))
  {
    return alloc_bytes<T>(sizeof(T) * count, alignment);
  }

  /**
   * Get current allocation pointer.
   *
   * \note Since this method returns a snapshot of the internal allocator
   *       state, it is inherently not thread-safe (regardless of the locking
   *       policy).
   *
   * \return Pointer to the memory where next allocation is going to happen.
   */
  void *ptr() const
  {
    return reinterpret_cast<void *>(_ptr);
  }

  /**
   * Get remaining allocator capacity.
   *
   * \note Since this method returns a snapshot of the internal allocator
   *       state, it is inherently not thread-safe (regardless of the locking
   *       policy).
   *
   * \return Remaining capacity of the allocator.
   */
  size_t size() const
  {
    return _size;
  }

private:
  uintptr_t _ptr = 0;
  size_t _size = 0;

  /**
   * Mutual exclusion lock.
   *
   * Must be declared as the last member to allow eliding it to zero size
   * using a flexible array type in \ref Lockless_policy.
   */
  typename LOCKING_POLICY::Lock_type _lock;
};
