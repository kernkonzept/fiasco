// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "static.h"
#include "static_i.h"

#line 4 "static.cpp"

extern "C" {
  extern char _mappings_1, _mappings_end_1;
}
#line 8 "static.cpp"

static const vm_offset_t mem_alloc_region
  = reinterpret_cast<vm_offset_t>(&_mappings_1);
#line 11 "static.cpp"

static const vm_offset_t mem_alloc_region_end
  = reinterpret_cast<vm_offset_t>(&_mappings_end_1);
#line 14 "static.cpp"

static kmem_slab_t *amm_entry_cache;
#line 16 "static.cpp"

static amm_t region_amm;
#line 18 "static.cpp"
static oskit_addr_t end_of_last_region;
#line 19 "static.cpp"
static helping_lock_t region_lock;
#line 20 "static.cpp"

static char keymap[128][2] = {
  {'[',	'{'},
//   {']',	'}'},		/* 27 */
  {'+',	'*'},		/* 27 */
};
