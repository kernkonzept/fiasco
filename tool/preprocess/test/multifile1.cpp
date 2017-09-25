INTERFACE:

class Foo
{
};

IMPLEMENTATION [part1]:

PUBLIC
void 
Foo::bar ()
{}

PROTECTED inline NEEDS [Foo::rambo]
void
Foo::baz ()
{}

IMPLEMENTATION:

PRIVATE
void
Foo::gizmatic ()
{}
