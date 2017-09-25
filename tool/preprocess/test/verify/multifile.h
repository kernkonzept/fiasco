// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef multifile_h
#define multifile_h

//
// INTERFACE definition follows 
//

#line 7 "multifile2.cpp"

class Bar;
#line 2 "multifile1.cpp"

class Foo
{

#line 2 "multifile2.cpp"
private:

  int more;

public:  
#line 10 "multifile1.cpp"
  void 
  bar ();
  
#line 15 "multifile2.cpp"
  void 
  bingo (Bar* bar);

protected:  
#line 15 "multifile1.cpp"
  inline void
  baz ();
  
#line 20 "multifile2.cpp"
  inline void 
  rambo ();

private:  
#line 22 "multifile1.cpp"
  void
  gizmatic ();
};

//
// IMPLEMENTATION of inline functions (and needed classes)
//


#line 18 "multifile2.cpp"


inline void 
Foo::rambo ()
{}

#line 13 "multifile1.cpp"


inline void
Foo::baz ()
{}

#endif // multifile_h
