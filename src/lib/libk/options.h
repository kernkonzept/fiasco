#pragma once

#include <cxx/type_traits>

class Reschedule
{
public:
  enum class Enum { No, Yes };
  using enum Enum;

  constexpr Reschedule(Enum v) : _value(v) {}

  friend constexpr bool operator==(Reschedule const &lhs, Reschedule const &rhs)
  { return lhs._value == rhs._value; }

  constexpr Reschedule &operator|=(Reschedule const &rhs)
  {
    if (_value == No)
      _value = rhs._value;
    return *this;
  }

  static constexpr Reschedule when(bool b)
  { return Reschedule { b ? Yes : No }; }
  template<typename T> requires (cxx::is_integral_v<T>)
  static constexpr Reschedule if_not_zero(T val) { return when(val != T{0}); }
  template<typename T> requires (cxx::is_integral_v<T>)
  static constexpr Reschedule if_zero(T val) { return when(val == T{0}); }

private:
  Enum _value;
};

