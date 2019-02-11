IMPLEMENTATION[!debug]:

#include <cstdio>

inline NEEDS [<cstdio>]
void kdb_ke(const char *msg)
{
  printf("NO JDB: %s\n"
         "So go ahead.\n", msg);
}

inline void kdb_ke_sequence(const char *) {}
