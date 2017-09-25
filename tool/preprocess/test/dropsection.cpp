INTERFACE:

class Gen_foo
{
public:
  int baz;

protected:
  int foo( int );
  
};

class Gen_foo_ext : private Gen_foo, public Ixdebix
{
protected:
  unsigned stuff;
};

IMPLEMENTATION:

PUBLIC inline void
Gen_foo::bar( unsigned x )
{
  // do soemthing with x;
}

PUBLIC void 
Gen_foo_ext::test()
{
  // the test
}

IMPLEMENTATION [ixbix-bax]:

IMPLEMENT int
Gen_foo::foo( int y )
{
  // do something strange with y
  bar(y);
  return y;
}

IMPLEMENTATION [aba-bax]:

IMPLEMENT int
Gen_foo::foo( int y )
{
  // just return y
  return y;
}

IMPLEMENTATION [{!bax,!ixbix}]:

PUBLIC int
Gen_foo::tust( int y )
{
  // just return y
  return y;
}

