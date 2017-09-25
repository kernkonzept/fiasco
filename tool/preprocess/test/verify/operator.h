// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef operator_h
#define operator_h

//
// INTERFACE definition follows 
//

#line 2 "operator.cpp"

class Foo
{

public:  
#line 10 "operator.cpp"
  void * 
  operator new (size_t);	// funny comment
  
  
#line 16 "operator.cpp"
  Foo&
  operator+ (const Foo&);	// funny comment
  
  
#line 22 "operator.cpp"
  Foo&
  operator= (const Foo&);	// funny comment
  
  
#line 28 "operator.cpp"
  Foo&
  operator* (const Foo&);	// funny comment
  
};

#line 34 "operator.cpp"
template <typename T, typename A> std::vector<T, A >& 
operator<< (std::vector<T, A>& in, const T& new_elem);

//
// IMPLEMENTATION of function templates
//


#line 32 "operator.cpp"


template <typename T, typename A> std::vector<T, A >& 
operator<< (std::vector<T, A>& in, const T& new_elem)
{
  in.push_back (new_elem);
  return in;
}

#endif // operator_h
