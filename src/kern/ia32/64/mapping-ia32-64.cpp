INTERFACE [amd64]:

#include "types.h"
class Space;
class Treemap;

class Mapping_entry
{
public:
  enum { Alignment = 1 };
  union 
  {
    struct 
    {
      unsigned long _space:32;	///< Address-space number
/*      unsigned long _pad:1;*/
      unsigned long address:36;	///< Virtual address in address space
    } data;
    Treemap *_submap;
  };
  Unsigned8 _depth;

  void set_space(Space *s) { data._space = (unsigned long)s & 0x0ffffffffUL; }
  Space *space() const
  { return (Space*)(0xffffffff00000000UL | (unsigned long)data._space); }
} __attribute__((packed));


