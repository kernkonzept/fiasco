IMPLEMENTATION[ia32,amd64]:

#include <cstdio>

inline NEEDS [<cstdio>]
bool kdb_ke(const char *msg)
{
  printf("NO KDB: %s\n"
	 "So go ahead.\n",msg);
  return false;
}
