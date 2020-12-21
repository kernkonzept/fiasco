INTERFACE:

#include "mapping_tree.h"

class Mappable : public Base_mappable
{
public:
  virtual ~Mappable() = 0;
};

IMPLEMENTATION:

IMPLEMENT inline Mappable::~Mappable() {}

