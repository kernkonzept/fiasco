INTERFACE:

#include "atomic.h"
#include "l4_types.h"

class Receiver;

/// Helper class to encode and decode value stored in reply cap slot.
class Reply_cap
{
  friend class Reply_cap_slot;

public:
  constexpr Reply_cap() : raw(0) {}

  Reply_cap(Receiver *caller, L4_fpage::Rights rights)
  : raw(reinterpret_cast<Mword>(caller)
        | (cxx::int_value<L4_fpage::Rights>(rights) & 0x3UL))
  {}

  bool constexpr is_valid() const noexcept { return raw != 0; }
  explicit constexpr operator bool() const { return is_valid(); }

  Receiver *caller() const
  { return reinterpret_cast<Receiver*>(raw & ~0x03UL); }

  constexpr L4_fpage::Rights caller_rights() const
  { return L4_fpage::Rights(raw & 0x03UL); }

  static constexpr Reply_cap Empty() { return Reply_cap(); }

private:
  Mword raw;
};

/// A reply cap slot. Used for implicit and explicit reply cap slots.
class Reply_cap_slot
{
public:
  Reply_cap_slot() = default;

  // Must not be copied or moved because we hold pointers to the slots!
  Reply_cap_slot(Reply_cap_slot const &) = delete;
  Reply_cap_slot &operator=(Reply_cap_slot const &) = delete;

  /// Can be used heuristically.
  bool is_used() const { return _slot.is_valid(); }

  bool cas(Reply_cap oldval, Reply_cap newval)
  {
    return ::cas(&_slot.raw, oldval.raw, newval.raw);
  }

  Reply_cap read_dirty() const { return _slot; }
  Reply_cap read_atomic() const { return atomic_load(&_slot); }

  void write_atomic(Reply_cap newval)
  { atomic_store(&_slot, newval); }

  /// Atomically replace Reply_cap with an empty one.
  Reply_cap reset_atomic()
  { return atomic_exchange(&_slot, Reply_cap::Empty()); }

private:
  Reply_cap _slot;
};
