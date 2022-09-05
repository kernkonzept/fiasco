// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef drop_multi2_h
#define drop_multi2_h

//
// INTERFACE definition follows
//

#line 2 "dropsection.cpp"

class Gen_foo

#line 7 "dropsection-ext.cpp"
                        : public Gen_bar

#line 4 "dropsection.cpp"
{
public:
  int baz;

protected:
  int foo( int );
  

private: // EXTENSION

#line 8 "dropsection-ext.cpp"

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
#line 21 "dropsection-ext.cpp"
  int
  do_something_private();
};
#line 12 "dropsection.cpp"

class Gen_foo_ext : private Gen_foo, public Ixdebix

#line 13 "dropsection-ext.cpp"
                            , public Gen_baz

#line 14 "dropsection.cpp"
{
protected:
  unsigned stuff;

private: // EXTENSION

#line 14 "dropsection-ext.cpp"

private:
  char *extension;

public:  
#line 27 "dropsection.cpp"
  void 
  test();
};

#endif // drop_multi2_h
