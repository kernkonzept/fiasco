INTERFACE:

struct Foo
{
  struct Bar 
  {
    int baz;
  };

public:
  int alreadythere();
  inline int alsoalreadythere();
};

IMPLEMENTATION:

#include "foo.h"

inline 
int bar()
{}

// Try multiline NEEDS
inline NEEDS["foo.h",
	      bar]
int baz()
{}

// Try function arguments
unsigned
somefunc(unsigned (*func1)(), 
	 unsigned (*func2)())
{
}

// Try function-pointer typedef
typedef int (* diag_printf_t) (const char *, ...);
typedef int (**dblfptr) (void);
typedef int (* arrfptr[20]) (void);

// Try to initialize a nested structure object.
struct Foo::Bar some_bar = { 1 };

// And add a Foo function
void Foo::func ()
{}

// Try default arguments
void Foo::bar (int i = 15, int j = somefunc(0, somefunc(0, 0)))
{}

// Try a constructor with weird syntax
PUBLIC 
Foo::Foo ()
  : something (reinterpret_cast<Bar*>(Baz::bla()))
{}

// Try implementing an already-declared function
IMPLEMENT int 
Foo::alreadythere()
{}

IMPLEMENT inline int 
Foo::alsoalreadythere()
{}

// 
// Try some commented-out code -- only #if 0 supported at the moment.
// 
#if 0

#ifdef FOO
funny
#else
even funnier
#endif

#else  // ! 0

void find_this ();

#if 0
but not this
#endif

#ifdef HEILIGE_WEIHNACHT
static void present_this ();
#endif

#endif
