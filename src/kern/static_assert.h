#ifndef __STATIC_ASSERT_H__
#define __STATIC_ASSERT_H__

#ifndef __GXX_EXPERIMENTAL_CXX0X__
#define static_assert(x, y) \
  do { (void)sizeof(char[-(!(x))]); } while (0)
#endif

#endif
