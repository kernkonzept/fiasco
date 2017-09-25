// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef drop_multi2_h
#define drop_multi2_h

//
// INTERFACE definition follows 
//

#line 2 "dropsection.cpp"

class Gen_foo

: public Gen_bar
{
public:
  int baz;

protected:
  int foo( int );
  

#line 2 "dropsection-ext.cpp"
private:

public:
  int extra_var;

public:  
#line 21 "dropsection.cpp"
  void
  bar( unsigned x );
  
#line 54 "dropsection.cpp"
  int
  tust( int y );

private:  
#line 17 "dropsection-ext.cpp"
  int
  do_something_private();
};
#line 12 "dropsection.cpp"

class Gen_foo_ext : private Gen_foo, public Ixdebix

, public Gen_baz
{
protected:
  unsigned stuff;

#line 8 "dropsection-ext.cpp"
private:

private:
  char *extension;

public:  
#line 27 "dropsection.cpp"
  void 
  test();
};

#endif // drop_multi2_h
