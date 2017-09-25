INTERFACE:

IMPLEMENTATION:

extern "C" {
  extern char _mappings_1, _mappings_end_1;
}

static const vm_offset_t mem_alloc_region
  = reinterpret_cast<vm_offset_t>(&_mappings_1);

static const vm_offset_t mem_alloc_region_end
  = reinterpret_cast<vm_offset_t>(&_mappings_end_1);

static kmem_slab_t *amm_entry_cache;

static amm_t region_amm;
static oskit_addr_t end_of_last_region;
static helping_lock_t region_lock;

static char keymap[128][2] = {
  {'[',	'{'},
//   {']',	'}'},		/* 27 */
  {'+',	'*'},		/* 27 */
};
