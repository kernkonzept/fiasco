// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "inline_noinline.h"
#include "inline_noinline_i.h"


#line 83 "inline.cpp"


constexpr void
Foo::cexpr2()
{
}

#line 24 "inline.cpp"


bool
Frob::private_func()
{
}

#line 30 "inline.cpp"


bool 
Foo::private_func()
{
}

#line 36 "inline.cpp"


bool 
Bar::private_func()
{
}

#line 48 "inline.cpp"


void 
Bar::another_private_func()
{
}

#line 54 "inline.cpp"


void
Bar::public_func()
{
}

#line 60 "inline.cpp"

// This inline funtion is public only because it is needed by an
// extern-"C" function.  So we do not want to export it.

void 
Foo::bar()
{

}

#line 69 "inline.cpp"

// Try both NOEXPORT and NEEDED.

void 
Foo::baz()
{
}

#line 101 "inline.cpp"

extern "C" 
void function (Foo* f)
{
  f->bar();
  f->cexpr2();
}
