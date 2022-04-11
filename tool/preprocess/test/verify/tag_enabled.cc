// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "tag_enabled.h"
#include "tag_enabled_i.h"


//
// IMPLEMENTATION follows
//


#line 8 "tag_enabled.cpp"


bool T::function1(void)
{
  return true /* = TAG_ENABLED(tag_defined) */;
}

#line 14 "tag_enabled.cpp"

bool T::function2(void)
{
  return false /* = TAG_ENABLED(tag_not_defined) */;
}

#line 19 "tag_enabled.cpp"

bool T::function3(void)
{
  return false /* = TAG_ENABLED(tag_not_defined && tag_defined) */;
}

#line 24 "tag_enabled.cpp"

bool T::function4(void)
{
  return true /* = TAG_ENABLED(tag_not_defined || tag_defined) */;
}
