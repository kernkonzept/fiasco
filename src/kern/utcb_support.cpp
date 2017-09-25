INTERFACE:

#include "l4_types.h"

class Utcb_support
{
public:
  static void current(User<Utcb>::Ptr const &utcb);
};
