IMPLEMENTATION[!debug]:

#include <cstdio>

inline NEEDS [<cstdio>]
void kdb_ke(char const *msg)
{
  printf("NO JDB: %s\n"
         "So go ahead.\n", msg);
}

inline void kdb_ke_sequence(char const *, unsigned) {}
