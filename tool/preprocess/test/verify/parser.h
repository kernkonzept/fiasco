// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef parser_h
#define parser_h

//
// INTERFACE definition follows 
//

#line 2 "parser.cpp"

struct Foo
{
  struct Bar 
  {
    int baz;
  };

public:
  int alreadythere();
  inline int alsoalreadythere();

public:  
#line 52 "parser.cpp"
  // Try a constructor with weird syntax
  
  Foo ();

private:  
#line 44 "parser.cpp"
  // And add a Foo function
  void func ();
  
#line 48 "parser.cpp"
  // Try default arguments
  void bar (int i = 15, int j = somefunc(0, somefunc(0, 0)));
};

#line 20 "parser.cpp"
inline int bar();

#line 23 "parser.cpp"
// Try multiline NEEDS


inline int baz();

#line 29 "parser.cpp"
// Try function arguments
unsigned
somefunc(unsigned (*func1)(), 
	 unsigned (*func2)());

//
// IMPLEMENTATION includes follow (for use by inline functions/templates)
//

#line 16 "parser.cpp"

#include "foo.h"

//
// IMPLEMENTATION of inline functions (and needed classes)
//


#line 62 "parser.cpp"

inline int 
Foo::alsoalreadythere()
{}

#line 18 "parser.cpp"


inline int bar()
{}

#line 22 "parser.cpp"

// Try multiline NEEDS


inline int baz()
{}

#endif // parser_h
