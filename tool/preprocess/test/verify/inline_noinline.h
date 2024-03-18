// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef inline_noinline_h
#define inline_noinline_h

//
// INTERFACE definition follows 
//

#line 2 "inline.cpp"

class Foo
{

public:  
#line 61 "inline.cpp"
  // This inline funtion is public only because it is needed by an
  // extern-"C" function.  So we do not want to export it.
  
  void 
  bar();
  
#line 70 "inline.cpp"
  // Try both NOEXPORT and NEEDED.
  
  void 
  baz();
  
#line 78 "inline.cpp"
  constexpr void
  cexpr1();
  
#line 85 "inline.cpp"
  constexpr void
  cexpr2();

private:  
#line 32 "inline.cpp"
  bool 
  private_func();
};
#line 6 "inline.cpp"

class Bar
{

public:  
#line 56 "inline.cpp"
  void
  public_func();

private:  
#line 38 "inline.cpp"
  bool 
  private_func();
  
#line 50 "inline.cpp"
  void 
  another_private_func();
};
#line 10 "inline.cpp"

class Cexpr
{
  constexpr test1();
};

#line 102 "inline.cpp"
extern "C" 
void function (Foo* f);

#line 109 "inline.cpp"
template <typename T> void* xcast(T* t);

//
// IMPLEMENTATION of inline functions (and needed classes)
//


#line 76 "inline.cpp"


constexpr void
Foo::cexpr1()
{

}

#line 89 "inline.cpp"


constexpr void
Cexpr::test1()
{
}

#line 95 "inline.cpp"


constexpr void
Cexpr::test2()
{
}

//
// IMPLEMENTATION of function templates
//


#line 108 "inline.cpp"

 template <typename T> void* xcast(T* t)
{
  return (void*) t;
}

#endif // inline_noinline_h
