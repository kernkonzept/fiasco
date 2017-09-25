INTERFACE:

class Test
{
};

IMPLEMENTATION:

PUBLIC
void
Test::func1() const throw()
{}

PUBLIC
void
Test::func2() throw(int, char const *) override final
{};

PUBLIC
void
Test::func3(unsigned) noexcept final override
{}

PUBLIC
void
Test::func4(int) noexcept ( this->func2() )
{}

PUBLIC explicit
Test::Test(int) noexcept(true) __attribute__ (( test("str") )) final
: init(This)
{}

PUBLIC
void
Test::func5() noexcept ( this->func2() ) [[test(attr)]] [[test2]]
{}

PUBLIC inline
int attribute((foo("murx("))) [[this->is->a->test]]
Test::func_with_stupid_attributes(Tmpl<Type> x) const && throw() = default;

/**
 * This is the comment for `free_function`.
 */
Test const &
free_function(int, long a) noexcept
{
  return Test(a);
}
