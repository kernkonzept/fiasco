// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef inline_h
#define inline_h

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
  
  inline void 
  bar();
  
#line 70 "inline.cpp"
  // Try both NOEXPORT and NEEDED.
  
  inline void 
  baz();
  
#line 78 "inline.cpp"
  constexpr void
  cexpr1();
  
#line 85 "inline.cpp"
  constexpr void
  cexpr2();

private:  
#line 32 "inline.cpp"
  inline bool 
  private_func();
};
#line 6 "inline.cpp"

class Bar
{

public:  
#line 56 "inline.cpp"
  inline void
  public_func();

private:  
#line 38 "inline.cpp"
  inline bool 
  private_func();
  
#line 50 "inline.cpp"
  inline void 
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
template <typename T> inline void* xcast(T* t);

//
// IMPLEMENTATION of inline functions (and needed classes)
//

#line 18 "inline.cpp"

// Test dependency-chain resolver

class Frob
{

private:  
#line 26 "inline.cpp"
  inline bool
  private_func();
  
#line 44 "inline.cpp"
  constexpr bool
  private_cexpr();
};

#line 30 "inline.cpp"


inline bool 
Foo::private_func()
{
}

#line 76 "inline.cpp"


constexpr void
Foo::cexpr1()
{

}

#line 36 "inline.cpp"


inline bool 
Bar::private_func()
{
}

#line 48 "inline.cpp"


inline void 
Bar::another_private_func()
{
}

#line 24 "inline.cpp"


inline bool
Frob::private_func()
{
}

#line 54 "inline.cpp"


inline void
Bar::public_func()
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

#line 108 "inline.cpp"

 template <typename T> inline void* xcast(T* t)
{
  return (void*) t;
}

#endif // inline_h
