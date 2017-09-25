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
#line 49 "inline.cpp"
  // This inline funtion is public only because it is needed by an
  // extern-"C" function.  So we do not want to export it.
  
  void 
  bar();
  
#line 58 "inline.cpp"
  // Try both NOEXPORT and NEEDED.
  
  void 
  baz();

private:  
#line 26 "inline.cpp"
  bool 
  private_func();
};
#line 6 "inline.cpp"

class Bar
{

public:  
#line 44 "inline.cpp"
  void
  public_func();

private:  
#line 32 "inline.cpp"
  bool 
  private_func();
  
#line 38 "inline.cpp"
  void 
  another_private_func();
};

#line 65 "inline.cpp"
extern "C" 
void function (Foo* f);

#line 71 "inline.cpp"
template <typename T> void* xcast(T* t);

//
// IMPLEMENTATION of function templates
//


#line 70 "inline.cpp"

 template <typename T> void* xcast(T* t)
{
  return (void*) t;
}

#endif // inline_noinline_h
