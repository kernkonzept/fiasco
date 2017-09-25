INTERFACE [io]:

#include "config.h"
#include "io_space.h"
#include "l4_types.h"

EXTENSION class Space : public Generic_io_space<Space>
{};

