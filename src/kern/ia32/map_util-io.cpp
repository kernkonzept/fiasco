/*
 * Fiasco-ia32
 * Specific code for I/O port protection
 */
INTERFACE[(ia32 || amd64) && io && debug]:

#include "mapdb.h"
#include "types.h"

extern Static_object<Mapdb> mapdb_io;

IMPLEMENTATION[(ia32 || amd64) && io]:

#include "l4_types.h"
#include "space.h"
#include "io_space.h"

Static_object<Mapdb> mapdb_io;

void init_mapdb_io(Space *sigma0)
{
  static size_t const io_page_sizes[] =
    {Io_space::Map_superpage_shift, 9, Io_space::Page_shift};

  mapdb_io.construct(sigma0, Mapping::Order(16), io_page_sizes, 3);
}

/**
 * Map the IO port region described by `fp_from` of address space `from` into
 * the address space `to`. IO ports can only be mapped idempotently, therefore
 * there is no offset for `fp_from` and only those ports are mapped that lay
 * in the intersection of `fp_from` and `fp_to`.
 *
 * \param from       Source address space.
 * \param fp_from    IO flexpage descripton for IO space range in source IO
 *                   space.
 * \param to         Destination address space.
 * \param fp_to      IO flexpage description for IO space range in destination
 *                   IO space.
 * \param reap_list  List of objects that need to be reaped in case of failure.
 *
 * \return IPC error code that describes the status of the operation.
 */
L4_error __attribute__((nonnull(1, 3)))
io_map(Space *from, L4_fpage const &fp_from,
       Space *to, L4_fpage const &fp_to, L4_snd_item control,
       Kobjects_list &reap_list)
{
/*   printf("io_map %u -> %u "
 *	    "snd %08x base %x size %x rcv %08x base %x size %x\n",
 *          (unsigned)from->space(), (unsigned)to->space(),
 *          fp_from.fpage,
 *          fp_from.iofp.iopage, fp_from.iofp.iosize,
 *          fp_to.fpage,
 *          fp_to.iofp.iopage, fp_to.iofp.iosize);
 *   kdb_ke("io_fpage_map 1");
 */

  Io_space::V_pfn rcv_addr = fp_to.io_address();
  Io_space::V_pfn snd_addr = fp_from.io_address();

  Order ro = Mu::get_order_from_fp<Io_space::V_pfc>(fp_to);
  Order so = Mu::get_order_from_fp<Io_space::V_pfc>(fp_from);

  snd_addr = cxx::mask_lsb(snd_addr, so);
  rcv_addr = cxx::mask_lsb(rcv_addr, ro);

  Io_space::V_pfc rcv_size = Io_space::V_pfc(1) << ro;
  Io_space::V_pfc snd_size = Io_space::V_pfc(1) << so;

  Io_space::V_pfn map_addr;
  if (rcv_addr > snd_addr)
    {
      if (rcv_addr - snd_addr >= snd_size)
        return L4_error::None;
      map_addr = rcv_addr;
    }
  else
    {
      if (snd_addr - rcv_addr >= rcv_size)
        return L4_error::None;
      map_addr = snd_addr;
    }

  // because of alignment, the overlap must be one of snd_size or rcv_size
  Io_space::V_pfc const map_size = snd_size < rcv_size ? snd_size : rcv_size;

  Mu::Auto_tlb_flush<Io_space> tlb;

  return map<Io_space>(mapdb_io.get(), from, from, map_addr, map_size, to, to,
                       map_addr, control.is_grant(), fp_from.rights(), tlb,
                       reap_list);
}

/**
 * Unmap IO mappings.
 *
 * Unmap the region described by `fp` from the IO space `space` and/or the IO
 * spaces the mappings have been mapped into.
 *
 * \param space      Address space that should be flushed.
 * \param fp         IO flexpage descriptor of IO-space range that should be
 *                   flushed.
 * \param reap_list  List of objects that need to be reaped.
 *
 * \retval #Page::Flags::None()
 */
Page::Flags __attribute__((nonnull(1)))
io_fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask,
               Kobjects_list &reap_list)
{
  Order size = Mu::get_order_from_fp<Io_space::V_pfc>(fp);
  Io_space::V_pfn port = fp.io_address();
  port = cxx::mask_lsb(port, size);

  // Here we _would_ reset IOPL to 0 but this doesn't make much sense
  // for only one thread since this thread may have forwarded the right
  // to other threads too. Therefore we had to walk through any thread
  // of this space.
  //
  // current()->regs()->eflags &= ~EFLAGS_IOPL;

  Mu::Auto_tlb_flush<Io_space> tlb;
  return unmap<Io_space>(mapdb_io.get(), space, space, port,
                         Io_space::V_pfc(1) << size, fp.rights(), mask, tlb,
                         reap_list);
}
