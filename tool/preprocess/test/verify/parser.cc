// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "parser.h"
#include "parser_i.h"


//
// IMPLEMENTATION follows
//

#line 46 "parser.cpp"

// Try to initialize a nested structure object.
struct Foo::Bar some_bar = { 1 };
#line 85 "parser.cpp"

void find_this ();
#line 91 "parser.cpp"

#ifdef HEILIGE_WEIHNACHT
#line 93 "parser.cpp"
static void present_this ();
#line 94 "parser.cpp"
#endif

#line 34 "parser.cpp"

// Try function arguments
unsigned
somefunc(unsigned (*func1)(), 
	 unsigned (*func2)())
{
}

#line 49 "parser.cpp"

// And add a Foo function
void Foo::func ()
{}

#line 53 "parser.cpp"

// Try default arguments
void Foo::bar (int i, int j)
{}

#line 57 "parser.cpp"

// Try a constructor with weird syntax

Foo::Foo ()
  : something (reinterpret_cast<Bar*>(Baz::bla()))
{}

#line 63 "parser.cpp"

// Try implementing an already-declared function
int 
Foo::alreadythere()
{}
