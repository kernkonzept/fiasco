INTERFACE:

class Foo
{
};

class Bar
{
};

IMPLEMENTATION:

// Test dependency-chain resolver

class Frob
{
};

inline 
bool
Frob::private_func()
{
}

inline 
bool 
Foo::private_func()
{
}

inline 
bool 
Bar::private_func()
{
}

inline NEEDS [Foo::private_func, Bar::private_func]
void 
Bar::another_private_func()
{
}

PUBLIC inline NEEDS [Bar::another_private_func, Frob::private_func]
void
Bar::public_func()
{
}

// This inline funtion is public only because it is needed by an
// extern-"C" function.  So we do not want to export it.
PUBLIC inline NOEXPORT
void 
Foo::bar()
{

}

// Try both NOEXPORT and NEEDED.
PUBLIC inline NOEXPORT NEEDS[Foo::private_func]
void 
Foo::baz()
{
}

extern "C" 
void function (Foo* f)
{
  f->bar();
}

template <typename T> inline void* xcast(T* t)
{
  return (void*) t;
}
