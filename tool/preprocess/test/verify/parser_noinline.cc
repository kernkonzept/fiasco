// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "parser_noinline.h"
#include "parser_noinline_i.h"

#line 40 "parser.cpp"

// Try to initialize a nested structure object.
struct Foo::Bar some_bar = { 1 };
#line 79 "parser.cpp"

void find_this ();
#line 85 "parser.cpp"

#ifdef HEILIGE_WEIHNACHT
#line 87 "parser.cpp"
static void present_this ();
#line 88 "parser.cpp"
#endif

#line 18 "parser.cpp"


int bar()
{}

#line 22 "parser.cpp"

// Try multiline NEEDS


int baz()
{}

#line 28 "parser.cpp"

// Try function arguments
unsigned
somefunc(unsigned (*func1)(), 
	 unsigned (*func2)())
{
}

#line 43 "parser.cpp"

// And add a Foo function
void Foo::func ()
{}

#line 47 "parser.cpp"

// Try default arguments
void Foo::bar (int i, int j)
{}

#line 51 "parser.cpp"

// Try a constructor with weird syntax

Foo::Foo ()
  : something (reinterpret_cast<Bar*>(Baz::bla()))
{}

#line 57 "parser.cpp"

// Try implementing an already-declared function
int 
Foo::alreadythere()
{}

#line 62 "parser.cpp"

int 
Foo::alsoalreadythere()
{}
