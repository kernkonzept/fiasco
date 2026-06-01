// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#pragma once

//
// INTERFACE definition follows
//

#line 2 "dropsection.cpp"

class Gen_foo
{
public:
  int baz;

protected:
  int foo( int );
  

public:  
#line 21 "dropsection.cpp"
  void
  bar( unsigned x );
};
#line 12 "dropsection.cpp"

class Gen_foo_ext : private Gen_foo, public Ixdebix
{
protected:
  unsigned stuff;

public:  
#line 27 "dropsection.cpp"
  void 
  test();
};
