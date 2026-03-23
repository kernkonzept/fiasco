#pragma once

#include "cxx/type_traits"

namespace cxx {

/**
 * Divides two integers, and rounds the result to the next largest integer if
 * the division yields a remainder.
 *
 * Examples:
 *    6 / 3 = 2
 *    7 / 3 = 3
 *   -7 / 3 = -2
 *
 * \param n  Numerator
 * \param d  Denominator
 *
 * \return ceil(n / d)
 */
template<typename N, typename D>
constexpr N
div_ceil(N const &n, D const &d)
{
  // Since C++11 the "quotient is truncated towards zero (fractional part is
  // discarded)". Thus a negative quotient is already ceiled, whereas a
  // positive quotient is floored. Furthermore, since C++11 the sign of the
  // % operator is no longer implementation defined, thus we can use n % d to
  // detect if the quotient is positive (n % d >= 0) and was truncated (n % d !=
  // 0). In that case, we add one to round to the next largest integer.
  return n / d + ((n % d) > 0 ? 1 : 0);
}

/**
 * Computes the binary logarithm of the given number.
 *
 * \param val  Number whose logarithm to compute, must be greater than zero.
 *
 * \return The binary logarithm of `val`.
 */
constexpr unsigned
log2u(unsigned val)
{
  return 8 * sizeof(val) - __builtin_clz(val) - 1;
}

constexpr unsigned
log2u(unsigned long val)
{
  return 8 * sizeof(val) - __builtin_clzl(val) - 1;
}

constexpr unsigned
log2u(unsigned long long val)
{
  return 8 * sizeof(val) - __builtin_clzll(val) - 1;
}

/**
 * Computes the number of bits needed to store the given number.
 *
 * \param val  Number whose bit width to compute.
 *
 * \return The bit width of `val`.
 */
constexpr unsigned
bit_width(unsigned val)
{
  if (val == 0)
    return 0;

  return 8 * sizeof(val) - __builtin_clz(val);
}

constexpr unsigned
bit_width(unsigned long val)
{
  if (val == 0)
    return 0;

  return 8 * sizeof(val) - __builtin_clzl(val);
}

constexpr unsigned
bit_width(unsigned long long val)
{
  if (val == 0)
    return 0;

  return 8 * sizeof(val) - __builtin_clzll(val);
}

/**
 * Computes the smallest integral power of two that is not smaller than the
 * given number.
 *
 * \param val  Number whose bit ceiling to compute.
 *
 * \return The bit ceiling of `val`.
 */
template<typename T>
requires Type_traits<T>::is_unsigned
constexpr T bit_ceil(T val)
{
  if (val <= T{1})
    return T{1};

  return T{1} << bit_width(T{val - 1});
}

}
