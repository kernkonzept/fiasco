// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "c-preproc.h"
#include "c-preproc_i.h"

#line 4 "c-preproc.cpp"

// set CS (missing from OSKIT)
#define set_cs(cs) 				\
  asm volatile					\
    ("ljmp %0,$1f \n1:"				\
     : : "i" (cs));

#line 10 "c-preproc.cpp"

void 
function (void)
{
#if 1
  bar ();
#else
  foo ();
#endif
}
