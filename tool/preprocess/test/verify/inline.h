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
#line 49 "inline.cpp"
  // This inline funtion is public only because it is needed by an
  // extern-"C" function.  So we do not want to export it.
  
  inline void 
  bar();
  
#line 58 "inline.cpp"
  // Try both NOEXPORT and NEEDED.
  
  inline void 
  baz();

private:  
#line 26 "inline.cpp"
  inline bool 
  private_func();
};
#line 6 "inline.cpp"

class Bar
{

public:  
#line 44 "inline.cpp"
  inline void
  public_func();

private:  
#line 32 "inline.cpp"
  inline bool 
  private_func();
  
#line 38 "inline.cpp"
  inline void 
  another_private_func();
};

#line 65 "inline.cpp"
extern "C" 
void function (Foo* f);

#line 71 "inline.cpp"
template <typename T> inline void* xcast(T* t);

//
// IMPLEMENTATION of inline functions (and needed classes)
//

#line 12 "inline.cpp"

// Test dependency-chain resolver

class Frob
{

private:  
#line 20 "inline.cpp"
  inline bool
  private_func();
};

#line 24 "inline.cpp"


inline bool 
Foo::private_func()
{
}

#line 30 "inline.cpp"


inline bool 
Bar::private_func()
{
}

#line 36 "inline.cpp"


inline void 
Bar::another_private_func()
{
}

#line 18 "inline.cpp"


inline bool
Frob::private_func()
{
}

#line 42 "inline.cpp"


inline void
Bar::public_func()
{
}

#line 70 "inline.cpp"

 template <typename T> inline void* xcast(T* t)
{
  return (void*) t;
}

#endif // inline_h
