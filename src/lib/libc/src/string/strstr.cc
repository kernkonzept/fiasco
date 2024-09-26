#include <string.h>
#include <stdint.h>

extern "C"
char*
strstr(const char *haystack, const char *needle)
{
  if (!haystack || !needle)
    return nullptr;

  do
    {
      bool found = true;
      for (size_t idx = 0; needle[idx] != '\0'; idx++)
        {
          if (haystack[idx] == '\0' || haystack[idx] != needle[idx])
            {
              // this was not the needle we were looking for
              found = false;
              break;
            }
        }

      if (found)
        return const_cast<char*>(haystack);

    }
  while (*haystack++ != '\0');

  return nullptr;
}
