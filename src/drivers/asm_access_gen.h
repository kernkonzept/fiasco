// vi:ft=cpp

#pragma once

#include <cxx/type_traits>

namespace Asm_access {

template<typename T>
concept supported_type = cxx::is_same_v<T, Unsigned8>
                         || cxx::is_same_v<T, Unsigned16>
                         || cxx::is_same_v<T, Unsigned32>
                         || cxx::is_same_v<T, Unsigned64>
                         || cxx::is_same_v<T, Mword>;

template <supported_type T>
inline T
read(T const *mem)
{
  return *reinterpret_cast<volatile T const *>(mem);
}

template <supported_type T>
void
write(T val, T *mem)
{
  *reinterpret_cast<volatile T *>(mem) = val;
}

inline Unsigned64 read_non_atomic(Unsigned64 const *mem)
{ return read(mem); }

inline void write_non_atomic(Unsigned64 val, Unsigned64 *mem)
{ return write(val, mem); }

}
