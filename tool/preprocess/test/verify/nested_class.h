// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef nested_class_h
#define nested_class_h

//
// INTERFACE definition follows 
//

#line 2 "nested_class.cpp"

class Outer
{
public:
  class Inner
  {
    void *test() const;

    template<typename R>
    R *test_template() const;
  };

  template<typename T>
  class Inner_template
  {
    T *test() const;

    template<typename R>
    R *test_template(T *a) const;
  };
};
#line 25 "nested_class.cpp"
#line 35 "nested_class.cpp"

//
// IMPLEMENTATION of function templates
//


#line 30 "nested_class.cpp"


template<typename R> R *
Outer::Inner::test_template() const
{ return 0; }

#line 35 "nested_class.cpp"


template<typename T> T *
Outer::Inner_template<T>::test() const
{ return 0; }

#line 40 "nested_class.cpp"


 template<typename T> template<typename R> R *
Outer::Inner_template<T>::test_template(T *a) const
{ return reinterpret_cast<R *>(a); }

#endif // nested_class_h
