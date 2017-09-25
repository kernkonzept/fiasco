// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "comment_in_string.h"
#include "comment_in_string_i.h"

#line 4 "comment_in_string.cpp"

int foo(char *s);

#line 6 "comment_in_string.cpp"

void irq_init(unsigned char master_base, unsigned char slave_base)
{
  if (!(foo(" -vmware"))
    {
      foo("outb_p(MASTER_OCW, 0xfb);	// unmask irq2");
    }
  else
    {
      foo("using normal pic mode \n");
    }
}

#line 18 "comment_in_string.cpp"

void bar()
{
  foo ("if (0) {");
  foo ("} // if(0)");
}
