// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef template_inline_i_h
#define template_inline_i_h
#line 200 "template.cpp"

// 
// Member templates
// 

class Foo
{
  template <typename T> T* goo(T* t);

public:  
#line 217 "template.cpp"
  template <typename T> T*
  bar (T* t);
};
#line 209 "template.cpp"

template <class Bar>
class TFoo
{

public:  
#line 231 "template.cpp"
  template <typename T> T*
  baz (T* t);
};

//
// IMPLEMENTATION of function templates
//


#line 214 "template.cpp"



template <typename T> T*
Foo::bar (T* t)
{
}

#line 221 "template.cpp"



template <typename T> T*
Foo::goo (T* t)
{
}

#line 228 "template.cpp"




template <class Bar> template <typename T> T*
TFoo::baz (T* t)
{
}

#endif // template_inline_i_h
