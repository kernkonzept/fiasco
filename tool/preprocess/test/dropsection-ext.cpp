INTERFACE [ext]:

/**
  * Preprocess should discard this comment and adjust the line directive
  * accordingly.
  */
EXTENSION class Gen_foo : public Gen_bar
{
public:
  int extra_var;
};

EXTENSION class Gen_foo_ext : public Gen_baz
{
private:
  char *extension;
};

IMPLEMENTATION [ext]:

PRIVATE int
Gen_foo::do_something_private()
{
  // just do it
}

