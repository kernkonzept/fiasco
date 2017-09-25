INTERFACE [debug]:

#include "mapdb.h"
#include "types.h"

extern Static_object<Mapdb> mapdb_mem;

IMPLEMENTATION:

#include "config.h"
#include "mapdb.h"
#include "mem_space.h"
#include <minmax.h>

Static_object<Mapdb> mapdb_mem;

/** Map the region described by "fp_from" of address space "from" into
    region "fp_to" at offset "offs" of address space "to", updating the
    mapping database as we do so.
    @param from source address space
    @param fp_from_{page, size, write, grant} flexpage description for
	virtual-address space range in source address space
    @param to destination address space
    @param fp_to_{page, size} flexpage descripton for virtual-address
	space range in destination address space
    @param offs sender-specified offset into destination flexpage
    @return IPC error code that describes the status of the operation
 */
L4_error __attribute__((nonnull(1, 3)))
mem_map(Space *from, L4_fpage const &fp_from,
        Space *to, L4_fpage const &fp_to, L4_msg_item control)
{
  assert_opt (from);
  assert_opt (to);

  typedef Mem_space::V_pfn Pfn;
  typedef Mem_space::V_pfc Pfc;
  typedef Mem_space::V_order Order;

  if (EXPECT_FALSE(fp_from.order() < L4_fpage::Mem_addr::Shift
                   || fp_to.order() < L4_fpage::Mem_addr::Shift))
    return L4_error::None;

  // loop variables
  Pfn rcv_addr = fp_to.mem_address();
  Pfn snd_addr = fp_from.mem_address();
  Pfn offs = Virt_addr(control.address());

  // calc size in bytes from power of twos
  Order so = Mu::get_order_from_fp<Pfc>(fp_from, L4_fpage::Mem_addr::Shift);
  Order ro = Mu::get_order_from_fp<Pfc>(fp_to, L4_fpage::Mem_addr::Shift);

  snd_addr = cxx::mask_lsb(snd_addr, so);
  rcv_addr = cxx::mask_lsb(rcv_addr, ro);
  Mu::free_constraint(snd_addr, so, rcv_addr, ro, offs);

  Mem_space::Attr attribs(fp_from.rights() | L4_fpage::Rights::U(), control.mem_type());

  Mu::Auto_tlb_flush<Mem_space> tlb;

  return map<Mem_space>(mapdb_mem.get(),
                        from, from, snd_addr,
                        Pfc(1) << so, to, to,
                        rcv_addr, control.is_grant(), attribs, tlb,
                        (Mem_space::Reap_list**)0);
}

/** Unmap the mappings in the region described by "fp" from the address
    space "space" and/or the address spaces the mappings have been
    mapped into.
    @param space address space that should be flushed
    @param fp    flexpage descriptor of address-space range that should
                 be flushed
    @param me_too If false, only flush recursive mappings.  If true,
                 additionally flush the region in the given address space.
    @param restriction Only flush specific task ID.
    @param flush_mode determines which access privileges to remove.
    @return combined (bit-ORed) access status of unmapped physical pages
*/
L4_fpage::Rights __attribute__((nonnull(1)))
mem_fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask)
{
  if (fp.order() < L4_fpage::Mem_addr::Shift)
    return L4_fpage::Rights(0);

  Mem_space::V_order o = Mu::get_order_from_fp<Mem_space::V_pfc>(fp, L4_fpage::Mem_addr::Shift);
  Mem_space::V_pfc size = Mem_space::V_pfc(1) << o;
  Mem_space::V_pfn start = fp.mem_address();

  start = cxx::mask_lsb(start, o);
  Mu::Auto_tlb_flush<Mem_space> tlb;
  return unmap<Mem_space>(mapdb_mem.get(), space, space,
               start, size,
               fp.rights(), mask, tlb, (Mem_space::Reap_list**)0);
}




/** The mapping database.
    This is the system's instance of the mapping database.
 */
void
init_mapdb_mem(Space *sigma0)
{
  enum { Max_num_page_sizes = 7 };
  static size_t page_sizes[Max_num_page_sizes]
    = { Config::SUPERPAGE_SHIFT - Config::PAGE_SHIFT, 0 };

  typedef Mem_space::Page_order Page_order;
  Page_order const *ps = Mem_space::get_global_page_sizes();
  unsigned idx = 0;
  unsigned phys_bits(Cpu::boot_cpu()->phys_bits());
  phys_bits = min(phys_bits, (unsigned)MWORD_BITS);

  Page_order last_bits(phys_bits);

  for (Page_order c = last_bits; c >= Page_order(Config::PAGE_SHIFT); --c)
    {
      c = *ps;
      // not more than 2K entries per MDB split array
      if (c < last_bits && (last_bits - c) > Page_order(12))
        c = last_bits - Page_order(12);
      else
        ++ps;
      printf("MDB: use page size: %u\n", Page_order::val(c));
      assert (idx < Max_num_page_sizes);
      page_sizes[idx++] = Page_order::val(c) - Config::PAGE_SHIFT;
      last_bits = c;
    }

  if (0)
    printf("MDB: phys_bits=%u levels = %u\n", Cpu::boot_cpu()->phys_bits(), idx);

  mapdb_mem.construct(sigma0,
      Mapping::Page(1U << (phys_bits - Config::PAGE_SHIFT - page_sizes[0])),
      page_sizes, idx);
}


