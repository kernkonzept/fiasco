INTERFACE:

IMPLEMENTATION:

// set CS (missing from OSKIT)
#define set_cs(cs) 				\
  asm volatile					\
    ("ljmp %0,$1f \n1:"				\
     : : "i" (cs));

void 
function (void)
{
#if 1
  bar ();
#else
  foo ();
#endif
}
