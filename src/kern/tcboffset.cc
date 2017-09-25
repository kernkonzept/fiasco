#include "irq.h"
#include "jdb_ktrace.h"
#include "thread.h"
#include "types.h"

bool dumpzero;

#define NAME_LEN 58

#if 1
#define DUMP_CAST_OFFSET(type, subtype) 		\
  ((unsigned long)((subtype *)((type *)(10))) - (unsigned long)10),
#endif

#define DUMP_OFFSET(prefix,name,offset) (offset), 

#define DUMP_BITSHIFT(prefix, value) (value),

#define GET_MEMBER_PTR(type,member) 					\
  ((unsigned long)(&(((type *) 1)->member)) - 1)

#define DUMP_MEMBER1(prefix,						\
                     type1,member1,					\
                     name) (GET_MEMBER_PTR (type1, member1)),

#define DUMP_CONSTANT(prefix, value) (value),
#define DUMP_THREAD_STATE(value) (value),

#if 0
#define DUMP_CAST_OFFSET(type, subtype, member) 		\
  (GET_MEMBER_PTR(type, member) - GET_MEMBER_PTR(subtype, member)),
#endif

/**
 * Calculates the logarithm base 2 from the given 2^n integer.
 * @param value the 2^n integer
 * @return the log base 2 of value, the exponent n
 */
int log2(int value)
{
  unsigned c = 0; // c will be lg(v)
  while (value >>= 1)
    c++;
  return c;
}


void offsets_func(char const **a, unsigned long const **b) 
{
static char const length[32] __attribute__((unused, section(".e_length"))) = 
  { sizeof(unsigned long), };
static unsigned long const offsets[] __attribute__((unused, section(".offsets"))) = 
{
#include "tcboffset_in.h"
};
  *a = length;
  *b = offsets;

}
