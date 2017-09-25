// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef func_defs_h
#define func_defs_h

//
// INTERFACE definition follows 
//

#line 2 "func_defs.cpp"

class Test
{

public:  
#line 10 "func_defs.cpp"
  void
  func1() const throw();
  
#line 15 "func_defs.cpp"
  void
  func2() throw(int, char const *) override final;
  
#line 20 "func_defs.cpp"
  void
  func3(unsigned) noexcept final override;
  
#line 25 "func_defs.cpp"
  void
  func4(int) noexcept ( this->func2() );
  
#line 30 "func_defs.cpp"
  explicit Test(int) noexcept(true) __attribute__ (( test("str") )) final;
  
#line 35 "func_defs.cpp"
  void
  func5() noexcept ( this->func2() ) [[test(attr)]] [[test2]];
  
#line 40 "func_defs.cpp"
  int attribute((foo("murx("))) [[this->is->a->test]]
  func_with_stupid_attributes(Tmpl<Type> x) const && throw() = default;
};

#line 43 "func_defs.cpp"
/**
 * This is the comment for `free_function`.
 */
Test const &
free_function(int, long a) noexcept;

#endif // func_defs_h
