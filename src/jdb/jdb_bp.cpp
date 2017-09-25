// Implementation for those who do not provide a breakpoint implementation
INTERFACE:

#include "types.h"

class Jdb_bp
{
public:
  static int instruction_bp_at_addr(Address) { return 0; }
};
