INTERFACE:

class T
{
};

IMPLEMENTATION:

PUBLIC
bool T::function1(void)
{
  return TAG_ENABLED(tag_defined);
}

bool T::function2(void)
{
  return TAG_ENABLED(tag_not_defined);
}

bool T::function3(void)
{
  return TAG_ENABLED(tag_not_defined && tag_defined);
}

bool T::function4(void)
{
  return TAG_ENABLED(tag_not_defined || tag_defined);
}
