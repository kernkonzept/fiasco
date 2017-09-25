INTERFACE:

class Test
{
public:
  void warn_func1();
  void warn_func2();
  void func1();
  void func2();
  void defl();

  // skip this as the test cannot deal with errors
  // void err_func1();
};

IMPLEMENTATION:

IMPLEMENT_DEFAULT void Test::warn_func1() { /* this is the default */ }
IMPLEMENT void Test::warn_func1()         { /* this is the override */ }

IMPLEMENT void Test::warn_func2()         { /* this is the override */ }
IMPLEMENT_DEFAULT void Test::warn_func2() { /* this is the default */ }

IMPLEMENT_DEFAULT void Test::func1()  { /* this is the default */ }
IMPLEMENT_OVERRIDE void Test::func1() { /* this is the override */ }

IMPLEMENT_OVERRIDE void Test::func2() { /* this is the override */ }
IMPLEMENT_DEFAULT void Test::func2()  { /* this is the default */ }

IMPLEMENT_DEFAULT void Test::defl()  { /* this is the default */ }

// skip this as the test cannot deal with errors
// IMPLEMENT_OVERRIDE void Test::err_func1() { /* this is the override */ }
