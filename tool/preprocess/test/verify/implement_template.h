// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef implement_template_h
#define implement_template_h

//
// INTERFACE definition follows 
//

#line 2 "implement_template.cpp"

template< typename T >
class Test
{
public:
  void test_func();

public:  
#line 23 "implement_template.cpp"
  // comment 2
   // comment 1
  template<
    typename X, // comment within template args list
    typename X2 // another comment in tl args
  > void __attribute__((deprecated))
  test_func2();
};

//
// IMPLEMENTATION of function templates
//


#line 12 "implement_template.cpp"



// comment
template< typename T > void __attribute__((deprecated))
Test<T>::test_func()
{
}

#line 20 "implement_template.cpp"



// comment 2
 // comment 1
template< typename T > template<
  typename X, // comment within template args list
  typename X2 // another comment in tl args
> void __attribute__((deprecated))
Test<T>::test_func2<X, X2>()
{
}

#endif // implement_template_h
