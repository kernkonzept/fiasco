// vi:ft=cpp
/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

namespace cxx {

class Null_type;

template<bool flag, typename T, typename F>
struct Select { using Type = T; };

template<typename T, typename F>
struct Select<false, T, F> { using Type = F; };

template<typename T, typename U>
class Conversion
{
  using S = char;

  class B { char dummy[2]; };

  static S test(U);
  static B test(...);
  static T make_T();

public:
  static constexpr bool exists = sizeof(test(make_T())) == sizeof(S);
  static constexpr bool two_way = exists && Conversion<U, T>::exists;
  static constexpr bool exists_2_way = two_way;
  static constexpr bool same_type = false;
};

template<>
class Conversion<void, void>
{
public:
  static constexpr bool exists = true;
  static constexpr bool two_way = true;
  static constexpr bool exists_2_way = two_way;
  static constexpr bool same_type = true;
};

template<typename T>
class Conversion<T, T>
{
public:
  static constexpr bool exists = true;
  static constexpr bool two_way = true;
  static constexpr bool exists_2_way = two_way;
  static constexpr bool same_type = true;
};

template<typename T>
class Conversion<void, T>
{
public:
  static constexpr bool exists = false;
  static constexpr bool two_way = false;
  static constexpr bool exists_2_way = two_way;
  static constexpr bool same_type = false;
};

template<typename T>
class Conversion<T, void>
{
public:
  static constexpr bool exists = false;
  static constexpr bool two_way = false;
  static constexpr bool exists_2_way = two_way;
  static constexpr bool same_type = false;
};

namespace TT
{
  template<typename U>
  struct Pointer_traits
  {
    using Pointee = Null_type;
    static constexpr bool value = false;
  };

  template<typename U>
  struct Pointer_traits<U *>
  {
    using Pointee = U;
    static constexpr bool value = true;
  };

  template<typename U>
  struct Ref_traits
  {
    using Referee = U;
    static constexpr bool value = false;
  };

  template<typename U>
  struct Ref_traits<U &>
  {
    using Referee = U;
    static constexpr bool value = true;
  };

  template<typename U>
  struct Add_ref { using Type = U &; };

  template<typename U>
  struct Add_ref<U &> { using Type = U; };

  template<typename U>
  struct PMF_traits
  { static constexpr bool value = false; };

  template<typename U, typename F>
  struct PMF_traits<U F:: *>
  { static constexpr bool value = true; };

  template<typename U>
  struct Is_unsigned
  { static constexpr bool value = false; };

  template<>
  struct Is_unsigned<unsigned>
  { static constexpr bool value = true; };

  template<>
  struct Is_unsigned<unsigned char>
  { static constexpr bool value = true; };

  template<>
  struct Is_unsigned<unsigned short>
  { static constexpr bool value = true; };

  template<>
  struct Is_unsigned<unsigned long>
  { static constexpr bool value = true; };

  template<>
  struct Is_unsigned<unsigned long long>
  { static constexpr bool value = true; };

  template<typename U>
  struct Is_signed
  { static constexpr bool value = false; };

  template<>
  struct Is_signed<signed>
  { static constexpr bool value = true; };

  template<>
  struct Is_signed<signed char>
  { static constexpr bool value = true; };

  template<>
  struct Is_signed<signed short>
  { static constexpr bool value = true; };

  template<>
  struct Is_signed<signed long>
  { static constexpr bool value = true; };

  template<>
  struct Is_signed<signed long long>
  { static constexpr bool value = true; };

  template<typename U>
  struct Is_int
  { static constexpr bool value = false; };

  template<>
  struct Is_int<char>
  { static constexpr bool value = true; };

  template<>
  struct Is_int<bool>
  { static constexpr bool value = true; };

  template<>
  struct Is_int<wchar_t>
  { static constexpr bool value = true; };

  template<typename U>
  struct Is_float
  { static constexpr bool value = false; };

  template<>
  struct Is_float<float>
  { static constexpr bool value = true; };

  template<>
  struct Is_float<double>
  { static constexpr bool value = true; };

  template<>
  struct Is_float<long double>
  { static constexpr bool value = true; };

  template<typename T>
  struct Const_traits
  {
    using Type = T;
    using Const_type = const T;
    static constexpr bool value = false;
  };

  template<typename T>
  struct Const_traits<const T>
  {
    using Type = T;
    using Const_type = const T;
    static constexpr bool value = true;
  };
};

template<typename T>
struct Type_traits
{
  static constexpr bool is_unsigned = TT::Is_unsigned<T>::value;
  static constexpr bool is_signed = TT::Is_signed<T>::value;

  static constexpr bool is_int = TT::Is_int<T>::value;
  static constexpr bool is_float = TT::Is_float<T>::value;

  static constexpr bool is_pointer = TT::Pointer_traits<T>::value;
  static constexpr bool is_pointer_to_member = TT::PMF_traits<T>::value;
  static constexpr bool is_reference = TT::Ref_traits<T>::value;

  static constexpr bool is_scalar = is_unsigned || is_signed || is_int
    || is_pointer || is_pointer_to_member || is_reference;

  static constexpr bool is_fundamental = is_unsigned || is_signed || is_float
    || Conversion<T, void>::same_type;

  static constexpr bool is_const = TT::Const_traits<T>::value;

  using Param_type
    = Select<is_scalar, T,
             typename TT::Add_ref<typename TT::Const_traits<T>::Const_type>::Type>::Type;

  using Pointee_type = TT::Pointer_traits<T>::Pointee;
  using Referee_type = TT::Ref_traits<T>::Referee;
  using Non_const_type = TT::Const_traits<T>::Type;
  using Const_type = TT::Const_traits<T>::Const_type;
};

};
