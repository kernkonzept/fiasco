#pragma once

#include "cxx/type_traits"

// trivial, used to terminate the variadic recursion
template<typename A>
inline A const &
min(A const &a)
{ return a; }

// matches with automatic argument type deduction
template<typename A, typename ...ARGS>
inline A const &
min(A const &a1, A const &a2, ARGS const &...a)
{
  return min((a1 <= a2) ? a1 : a2, a...);
}

// matches with explicit template type A
template<typename A, typename ...ARGS>
inline A const &
min(typename cxx::identity<A>::type const &a1,
    typename cxx::identity<A>::type const &a2,
    ARGS const &...a)
{
  return min<A>((a1 <= a2) ? a1 : a2, a...);
}

// trivial, used to terminate the variadic recursion
template<typename A>
inline A const &
max(A const &a)
{ return a; }

// matches with automatic argument type deduction
template<typename A, typename ...ARGS>
inline A const &
max(A const &a1, A const &a2, ARGS const &...a)
{ return max((a1 >= a2) ? a1 : a2, a...); }

// matches with explicit template type A
template<typename A, typename ...ARGS>
inline A const &
max(typename cxx::identity<A>::type const &a1,
    typename cxx::identity<A>::type const &a2,
    ARGS const &...a)
{
  return max<A>((a1 >= a2) ? a1 : a2, a...);
}

