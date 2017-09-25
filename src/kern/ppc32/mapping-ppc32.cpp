INTERFACE [ppc32]:

#include "types.h"
class Treemap;

class Mapping_entry
{
public:
  enum { Alignment = 4 };
  union 
  {
    struct 
    {
      unsigned long _space:32;	///< Address-space number
//      unsigned long _pad:1;
      unsigned long address:20;	///< Virtual address in address space
    } __attribute__((packed)) data;
    Treemap *_submap;
  };
  Unsigned8 _depth;
  void set_space(Space *s) { data._space = (unsigned long)s; }
  Space *space() const { return (Space *)data._space; }
};

