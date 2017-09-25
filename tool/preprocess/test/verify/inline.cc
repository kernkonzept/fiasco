// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "inline.h"
#include "inline_i.h"


#line 48 "inline.cpp"

// This inline funtion is public only because it is needed by an
// extern-"C" function.  So we do not want to export it.

inline void 
Foo::bar()
{

}

#line 57 "inline.cpp"

// Try both NOEXPORT and NEEDED.

inline void 
Foo::baz()
{
}

#line 64 "inline.cpp"

extern "C" 
void function (Foo* f)
{
  f->bar();
}
