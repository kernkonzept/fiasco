INTERFACE [debug]:

#include "mapdb.h"
#include "types.h"
#include "global_data.h"

extern Global_data<Static_object<Mapdb>> mapdb_mem;

IMPLEMENTATION:

#include "config.h"
#include "cpu.h"
#include "mapdb.h"
#include "mem_space.h"
#include <minmax.h>
#include "global_data.h"

DEFINE_GLOBAL Global_data<Static_object<Mapdb>> mapdb_mem;

/**
 * Map the region described by `fp_from` of address space `from` into the
 * region `fp_to` at an offset defined by `control` of address space `to`,
 * updating the mapping database as we do so.
 *
 * \param from       Source address space.
 * \param fp_from    Flexpage description for virtual-address space range in
 *                   source address space (page, size, write, grant).
 * \param to         Destination address space.
 * \param fp_to      flexpage description for virtual-address space range in
 *                   destination address space (page, size).
 * \param control    Message send item containing the sender-specified offset
 *                   into the destination flexpage, the caching attributes and
 *                   the grant flag.
 * \param reap_list  List of objects that need to be reaped in case of failure.
 *
 * \return IPC error code that describes the status of the operation.
 */
L4_error __attribute__((nonnull(1, 3)))
mem_map(Space *from, L4_fpage const &fp_from,
        Space *to, L4_fpage const &fp_to, L4_snd_item control,
        Kobjects_list &reap_list)
{
  assert(from);
  assert(to);

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

  // No remapping possible without an MMU
  if constexpr (!Config::Have_mmu)
    {
      if (EXPECT_FALSE(snd_addr != rcv_addr))
        {
          WARN("No MMU: can't map from " L4_PTR_FMT " to " L4_PTR_FMT "\n",
               cxx::int_value<Pfn>(snd_addr), cxx::int_value<Pfn>(rcv_addr));
          return L4_error::Map_failed;
        }
    }

  Mem_space::Attr attribs(fp_from.rights() | L4_fpage::Rights::U(),
                          control.mem_type(), Page::Kern::None(),
                          Page::Flags::None());

  Mu::Auto_tlb_flush<Mem_space> tlb;

  return map<Mem_space>(mapdb_mem.get(),
                        from, from, snd_addr,
                        Pfc(1) << so, to, to,
                        rcv_addr, control.is_grant(), attribs, tlb, reap_list);
}

/**
 * Unmap the mappings in the region described by `fp` from the address space
 * `space` and/or the address spaces the mappings have been mapped into.
 *
 * \param space      Address space that should be flushed.
 * \param fp         Flexpage descriptor of address-space range that should
 *                   be flushed.
 * \param mask       Flags for unmap operation.
 * \param reap_list  List of objects that need to be reaped.
 *
 * \return Combined (bit-ORed) access flags of unmapped physical pages.
 */
Page::Flags __attribute__((nonnull(1)))
mem_fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask,
                Kobjects_list &reap_list)
{
  if (fp.order() < L4_fpage::Mem_addr::Shift)
    return Page::Flags::None();

  Mem_space::V_order o = Mu::get_order_from_fp<Mem_space::V_pfc>(fp, L4_fpage::Mem_addr::Shift);
  Mem_space::V_pfc size = Mem_space::V_pfc(1) << o;
  Mem_space::V_pfn start = fp.mem_address();

  start = cxx::mask_lsb(start, o);
  Mu::Auto_tlb_flush<Mem_space> tlb;

  return unmap<Mem_space>(mapdb_mem.get(), space, space,
               start, size, fp.rights(), mask, tlb, reap_list);
}

enum { Max_num_page_sizes = 7 };
static DEFINE_GLOBAL Global_data<size_t[Max_num_page_sizes]> page_sizes;

/** The mapping database.
    This is the system's instance of the mapping database.
 */
void
init_mapdb_mem(Space *sigma0)
{
  page_sizes[0] = unsigned{Config::SUPERPAGE_SHIFT} - unsigned{Config::PAGE_SHIFT};

  typedef Mem_space::Page_order Page_order;
  Page_order const *ps = Mem_space::get_global_page_sizes();
  unsigned idx = 0;
  unsigned phys_bits(Cpu::boot_cpu()->phys_bits());
  phys_bits = min<unsigned>(phys_bits, MWORD_BITS);

  Page_order last_bits(phys_bits);

  for (Page_order c = last_bits; c >= Page_order(Config::PAGE_SHIFT); --c)
    {
      c = *ps;
      // not more than 2K entries per MDB split array
      if (c < last_bits && (last_bits - c) > Page_order(12))
        c = last_bits - Page_order(12);
      else
        ++ps;
      printf("MDB: use page size: %u\n", cxx::int_value<Page_order>(c));
      assert (idx < Max_num_page_sizes);
      page_sizes[idx++] = cxx::int_value<Page_order>(c) - Config::PAGE_SHIFT;
      last_bits = c;
    }

  if constexpr (0) // Intentionally disabled, only used for diagnostics
    printf("MDB: phys_bits=%u levels = %u\n", Cpu::boot_cpu()->phys_bits(), idx);

  mapdb_mem.construct(sigma0,
                      Mapping::Order(phys_bits - Config::PAGE_SHIFT),
                      page_sizes, idx);
}


