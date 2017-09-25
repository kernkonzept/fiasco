INTERFACE:

class Outer
{
public:
  class Inner
  {
    void *test() const;
  };
};

IMPLEMENTATION:

IMPLEMENT inline
void *
Outer::Inner::test() const
{ return 0; }
