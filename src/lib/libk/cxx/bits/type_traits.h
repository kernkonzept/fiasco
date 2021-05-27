// vi:ft=cpp
/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */

#pragma once

namespace cxx {

class Null_type;

template< bool flag, typename T, typename F >
class Select 
{
public:
  typedef T Type;
};

template< typename T, typename F >
class Select< false, T, F > 
{
public:
  typedef F Type;
};
  


template< typename T, typename U >
class Conversion
{
  typedef char S;
  class B { char dummy[2]; };
  static S test(U);
  static B test(...);
  static T make_T();
public:
  enum 
  { 
    exists = sizeof(test(make_T())) == sizeof(S),
    two_way = exists && Conversion<U,T>::exists,
    exists_2_way = two_way,
    same_type = false
  };
};

template< >
class Conversion<void, void>
{
public:
  enum { exists = 1, two_way = 1, exists_2_way = two_way, same_type = 1 };
};

template< typename T >
class Conversion<T, T>
{
public:
  enum { exists = 1, two_way = 1, exists_2_way = two_way, same_type = 1 };
};

template< typename T >
class Conversion<void, T>
{
public:
  enum { exists = 0, two_way = 0, exists_2_way = two_way, same_type = 0 };
};

template< typename T >
class Conversion<T, void>
{
public:
  enum { exists = 0, two_way = 0, exists_2_way = two_way, same_type = 0 };
};

template< int I >
class Int_to_type
{
public:
  enum { i = I };
};

namespace TT
{
  template< typename U > class Pointer_traits 
  { 
  public: 
    typedef Null_type Pointee;
    enum { value = false };
  };

  template< typename U > class Pointer_traits< U* > 
  { 
  public: 
    typedef U Pointee;
    enum { value = true };
  };

  template< typename U > struct Ref_traits 
  { 
    enum { value = false };
    typedef U Referee;
  };
  
  template< typename U > struct Ref_traits<U&> 
  { 
    enum { value = true };
    typedef U Referee;
  };


  template< typename U > struct Add_ref { typedef U &Type; };
  template< typename U > struct Add_ref<U&> { typedef U Type; };

  template< typename U > struct PMF_traits { enum { value = false }; };
  template< typename U, typename F > struct PMF_traits<U F::*> 
  { enum { value = true }; };


  template< typename U > class Is_unsigned { public: enum { value = false }; };
  template<> class Is_unsigned<unsigned> { public: enum { value = true }; };
  template<> class Is_unsigned<unsigned char> { 
    public: enum { value = true }; 
  };
  template<> class Is_unsigned<unsigned short> { 
    public: enum { value = true }; 
  };
  template<> class Is_unsigned<unsigned long> { 
    public: enum { value = true }; 
  };
  template<> class Is_unsigned<unsigned long long> { 
    public: enum { value = true }; 
  };

  template< typename U > class Is_signed { public: enum { value = false }; };
  template<> class Is_signed<signed char> { public: enum { value = true }; };
  template<> class Is_signed<signed short> { public: enum { value = true }; };
  template<> class Is_signed<signed> { public: enum { value = true }; };
  template<> class Is_signed<signed long> { public: enum { value = true }; };
  template<> class Is_signed<signed long long> { 
    public: enum { value = true }; 
  };
  
  template< typename U > class Is_int { public: enum { value = false }; };
  template<> class Is_int< char > { public: enum { value = true }; };
  template<> class Is_int< bool > { public: enum { value = true }; };
  template<> class Is_int< wchar_t > { public: enum { value = true }; };
  
  template< typename U > class Is_float { public: enum { value = false }; };
  template<> class Is_float< float > { public: enum { value = true }; };
  template<> class Is_float< double > { public: enum { value = true }; };
  template<> class Is_float< long double > { public: enum { value = true }; };

  template<typename T> class Const_traits
  {
  public:
    enum { value = false };
    typedef T Type;
    typedef const T Const_type;
  };

  template<typename T> class Const_traits<const T>
  {
  public:
    enum { value = true };
    typedef T Type;
    typedef const T Const_type;
  };
};

template< typename T >
class Type_traits 
{
public:

  enum
  {
    is_unsigned = TT::Is_unsigned<T>::value,
    is_signed   = TT::Is_signed<T>::value,
    is_int      = TT::Is_int<T>::value,
    is_float    = TT::Is_float<T>::value,
    is_pointer  = TT::Pointer_traits<T>::value,
    is_pointer_to_member = TT::PMF_traits<T>::value,
    is_reference = TT::Ref_traits<T>::value,
    is_scalar = is_unsigned || is_signed || is_int || is_pointer 
      || is_pointer_to_member || is_reference,
    is_fundamental = is_unsigned || is_signed || is_float 
      || Conversion<T, void>::same_type,
    is_const    = TT::Const_traits<T>::value,

    alignment = 
	(sizeof(T) >= sizeof(unsigned long) 
	 ? sizeof(unsigned long)
	 : (sizeof(T) >= sizeof(unsigned)
	   ? sizeof(unsigned)
	   : (sizeof(T) >= sizeof(short)
	     ? sizeof(short)
	     : 1)))
  };

  typedef typename Select<is_scalar, T, typename TT::Add_ref<typename TT::Const_traits<T>::Const_type>::Type>::Type Param_type;
  typedef typename TT::Pointer_traits<T>::Pointee Pointee_type;
  typedef typename TT::Ref_traits<T>::Referee Referee_type;
  typedef typename TT::Const_traits<T>::Type Non_const_type;
  typedef typename TT::Const_traits<T>::Const_type Const_type;

  static unsigned long align(unsigned long a)
  { return (a + (unsigned long)alignment - 1UL) 
    & ~((unsigned long)alignment - 1UL); }
};


};



