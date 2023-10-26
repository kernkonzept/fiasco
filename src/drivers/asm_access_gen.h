// vi:ft=cpp

#pragma once

#include <cxx/type_traits>

namespace Asm_access {

template <typename T>
struct is_supported_type
{
  static const bool value = cxx::is_same<T, Unsigned8>::value
                            || cxx::is_same<T, Unsigned16>::value
                            || cxx::is_same<T, Unsigned32>::value
                            || cxx::is_same<T, Unsigned64>::value
                            || cxx::is_same<T, Mword>::value;
};

template <typename T>
inline
typename cxx::enable_if<is_supported_type<T>::value, T>::type
read(T const *mem)
{
  return *reinterpret_cast<volatile T const *>(mem);
}

template <typename T>
inline
typename cxx::enable_if<is_supported_type<T>::value, void>::type
write(T val, T *mem)
{
  *reinterpret_cast<volatile T *>(mem) = val;
}

inline Unsigned64 read_non_atomic(Unsigned64 const *mem)
{ return read(mem); }

inline void write_non_atomic(Unsigned64 val, Unsigned64 *mem)
{ return write(val, mem); }

}
