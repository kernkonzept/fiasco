/*
 * Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */

#include <assert.h>

int __aeabi_unwind_cpp_pr0(void);
int __aeabi_unwind_cpp_pr1(void);

int __aeabi_unwind_cpp_pr0(void)
{
  assert(0);
  return 0;
}

int __aeabi_unwind_cpp_pr1(void)
{
  assert(0);
  return 0;
}
