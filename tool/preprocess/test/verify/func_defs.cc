// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "func_defs.h"
#include "func_defs_i.h"


#line 8 "func_defs.cpp"


void
Test::func1() const throw()
{}

#line 13 "func_defs.cpp"


void
Test::func2() throw(int, char const *)  
{};

#line 18 "func_defs.cpp"


void
Test::func3(unsigned) noexcept  
{}

#line 23 "func_defs.cpp"


void
Test::func4(int) noexcept ( this->func2() )
{}

#line 28 "func_defs.cpp"


Test::Test(int) noexcept(true) __attribute__ (( test("str") )) 
: init(This)
{}

#line 33 "func_defs.cpp"


void
Test::func5() noexcept ( this->func2() ) [[test(attr)]] [[test2]]
{}

#line 42 "func_defs.cpp"

/**
 * This is the comment for `free_function`.
 */
Test const &
free_function(int, long a) noexcept
{
  return Test(a);
}
