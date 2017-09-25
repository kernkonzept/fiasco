INTERFACE:

#include "mapping_tree.h"

class Mappable : public Base_mappable
{
public:
  bool no_mappings() const;
  virtual ~Mappable() = 0;
};

IMPLEMENTATION:

IMPLEMENT inline Mappable::~Mappable() {}

IMPLEMENT
inline bool
Mappable::no_mappings() const
{
  return !tree.get() || tree.get()->is_empty();
}
