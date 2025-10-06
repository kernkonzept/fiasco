#pragma once

#include "types.h"

enum class Mapdb_lookup_src_dst
{
  /** Failure: 'Dst' not found, 'src' not found or both not found. */
  Fail,
  /** Success: 'Dst' is child mapping of 'src'. */
  Dst_child_of_src,
  /** Success: 'Dst' is equal to or ancestor of 'src'. */
  Dst_equal_to_or_ancestor_of_src,
  /** Success: 'Dst' and 'src' found and no other case applies (no anchestor
    * relation between dst and src) .*/
  Not_related,
};
