#pragma once

#include "types.h"

namespace Mdb_types {
  struct Order_t;
  struct Pcnt_t;
  struct Pfn_t;

  typedef cxx::int_type<unsigned, Order_t > Order;
  typedef cxx::int_type_order<Address, Pcnt_t, Order> Pcnt;
  typedef cxx::int_type_full<Address, Pfn_t, Pcnt, Order> Pfn;
}

