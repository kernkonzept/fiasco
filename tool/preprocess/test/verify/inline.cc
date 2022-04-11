// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "inline.h"
#include "inline_i.h"


//
// IMPLEMENTATION follows
//


#line 60 "inline.cpp"

// This inline funtion is public only because it is needed by an
// extern-"C" function.  So we do not want to export it.

inline void 
Foo::bar()
{

}

#line 69 "inline.cpp"

// Try both NOEXPORT and NEEDED.

inline void 
Foo::baz()
{
}

#line 83 "inline.cpp"


constexpr void
Foo::cexpr2()
{
}

#line 101 "inline.cpp"

extern "C" 
void function (Foo* f)
{
  f->bar();
  f->cexpr2();
}
