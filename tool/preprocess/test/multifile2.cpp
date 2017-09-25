INTERFACE:

EXTENSION class Foo 
{
  int more;
};

class Bar;

IMPLEMENTATION [part2]:

#include "bar.h"

PUBLIC
void 
Foo::bingo (Bar* bar)
{}

PROTECTED inline
void 
Foo::rambo ()
{}

