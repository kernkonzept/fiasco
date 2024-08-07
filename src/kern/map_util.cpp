INTERFACE:

#include "assert_opt.h"
#include "l4_types.h"
#include "space.h"
#include <cxx/function>
#include "cpu_call.h"
#include "tlbs.h"

class Mapdb;

namespace Mu {

template<typename SPACE>
struct Auto_tlb_flush
{
  void add_page(SPACE *, typename SPACE::V_pfn, typename SPACE::Page_order) {}
};

template<>
struct Auto_tlb_flush<Mem_space>
{
private:
  template<unsigned NUM_SPACES>
  struct Flush_store
  {
    enum { N_spaces = NUM_SPACES };
    bool all = false;
    bool empty = true;

    // When we store a pointer to a Mem_space we have to increment its reference
    // counter to ensure that it still exists, i.e. has not been deleted, when
    // we later dereference the pointer.
    struct Mem_space_ref_ptr : Ref_ptr<Space>
    {
      Mem_space_ref_ptr() = default;
      explicit Mem_space_ref_ptr(Mem_space *p)
      : Ref_ptr(static_cast<Space *>(p)) {}

      Mem_space *get() const { return Ref_ptr<Space>::get(); }
      Mem_space *operator -> () const { return get(); }
    };

    Mem_space_ref_ptr spaces[N_spaces];

    Flush_store()
    {}

    void add_space(Mem_space *space)
    {
      if (all)
        return;

      empty = false;

      for (unsigned i = 0; i < N_spaces; ++i)
        {
          if (spaces[i].get() == nullptr)
            {
              spaces[i] = Mem_space_ref_ptr(space);
              return;
            }

          if (spaces[i].get() == space)
            // Memory space is already registered
            return;
        }

      // got an overflow, we have to flush all
      all = true;
    }
  };

  struct Cpu_flush_store : public Flush_store<4>
  {
    void add_space(Mem_space *space)
    {
      Flush_store<4>::add_space(space);

      if (Mem_space::Need_xcpu_tlb_flush && all)
        {
          // When we run out of spaces, we have to record the affected CPUs
          // directly, which depending on the architecture requires an expensive
          // synchronization operation for each space added.
          //
          // But we have no choice, falling back sending an IPI to all CPUs is
          // not an option. We have to ensure that IPIs are only sent to CPUs on
          // which at least one of the involved memory spaces is active.
          // Otherwise it is not possible to safely partition the system,
          // because for example one partition could influence the other
          // partition by causing TLB flush induced IPI storms.
          Mem_space::sync_read_tlb_active_on_cpu();
          _all_affected_cpus |= space->tlb_active_on_cpu();
        }
    }

    /*
     * \pre cpu == current_cpu()
     */
    void flush_stored(Cpu_number cpu)
    {
      for (unsigned i = 0; i < N_spaces && spaces[i]; ++i)
        {
          if (!Mem_space::Need_xcpu_tlb_flush || spaces[i]->tlb_active_on_cpu().get(cpu))
            spaces[i]->tlb_flush_current_cpu();
        }
    }

    Cpu_mask affected_cpus() const
    {
      Cpu_mask affected_cpus;
      for (unsigned i = 0; i < N_spaces && spaces[i]; ++i)
        affected_cpus |= spaces[i]->tlb_active_on_cpu();

      if (all)
        affected_cpus |= _all_affected_cpus;

      return affected_cpus;
    }

  private:
    // Fallback to record affected CPUs for spaces that did not fit
    // into the spaces array.
    Cpu_mask _all_affected_cpus;
  };

  struct Iommu_flush_store : public Flush_store<4>
  {
    void flush_stored()
    {
      for (unsigned i = 0; i < N_spaces && spaces[i]; ++i)
        spaces[i]->tlb_flush_current_cpu();
    }
  };

  Cpu_flush_store _cpu_tlb;
  Iommu_flush_store _iommu_tlb;

  /*
   * This is called on the local CPU and target/remote CPUs to flush
   * CPU-local TLBs.
   *
   * \pre cpu == current_cpu()
   */
  void do_flush_cpu_op(Cpu_number cpu)
  {
    if (EXPECT_FALSE(_cpu_tlb.all))
      {
        // flush all CPU-local TLBs, e.g. MMU, ept, npt
        Tlb::flush_all_cpu(cpu);
        Mem_space::reload_current();
      }
    else
      _cpu_tlb.flush_stored(cpu);
  }

  /*
   * This is called on the CPU that made the changes requiring a TLB Flush,
   * to flush the affected CPU-local TLBs on all affected CPUs.
   */
  void do_flush_cpu()
  {
    if (_cpu_tlb.empty)
      return;

    if (Mem_space::Need_xcpu_tlb_flush)
      {
        // To prevent a race condition that could potentially lead to the use of
        // outdated page table entries on other cores, we have to execute a
        // memory barrier that ensures that our PTE changes are visible to all
        // other cores, before we access tlb_active_on_cpu(). Otherwise, if the
        // Mem_space gets active on another core, shortly after we read
        // tlb_active_on_cpu() where it was reported as non-active, we won't
        // send a TLB flush to the other core, but it might not yet see our PTE
        // changes.
        Mem_space::sync_read_tlb_active_on_cpu();

        Cpu_mask affected_cpus = _cpu_tlb.affected_cpus();

        // Limit to CPUs for which TLB management is enabled. If TLB management
        // gets deactivated for a CPU, this is always combined with a full flush
        // of all CPU-dependent TLBs. Therefore we can skip flushing on this CPU
        // here without any risk of stale TLB entries remaining.
        affected_cpus &= Mem_space::active_tlb();

        Cpu_call::cpu_call_many(affected_cpus, [this](Cpu_number cpu) {
          this->do_flush_cpu_op(cpu);
          return false;
        });
      }
    else
      do_flush_cpu_op(current_cpu());
  }

  /*
   * This is called on the CPU that made the changes requiring a TLB Flush,
   * to flush the affected IOMMU TLBs.
   */
  void do_flush_iommu()
  {
    if (EXPECT_TRUE(_iommu_tlb.empty))
      return;

    if (EXPECT_FALSE(_iommu_tlb.all))
      {
        // flush all CPU-independent TLBs, e.g IOMMU
        Tlb::flush_all_iommu();
      }
    else
      _iommu_tlb.flush_stored();
  }

public:
  void add_page(Mem_space *space, Mem_space::V_pfn, Mem_space::Page_order)
  {
    /* At a later stage, also use the addresses given here. */

    if (EXPECT_FALSE(space->tlb_type() == Mem_space::Tlb_iommu))
      _iommu_tlb.add_space(space);
    else
      _cpu_tlb.add_space(space);
  }

  ~Auto_tlb_flush()
  {
    do_flush_cpu();
    do_flush_iommu();
  }
};


struct Mapping_type_t;
typedef cxx::int_type<unsigned, Mapping_type_t> Mapping_type;

template< typename SPACE, typename M, typename O >
inline
L4_fpage::Rights
v_delete(M *m, O order, L4_fpage::Rights flush_rights,
         [[maybe_unused]] bool full_flush, Auto_tlb_flush<SPACE> &tlb)
{
  SPACE* child_space = m->space();
  assert_opt (child_space);
  L4_fpage::Rights res = child_space->v_delete(SPACE::to_virt(m->pfn(order)),
                                               SPACE::to_order(order),
                                               flush_rights);
  tlb.add_page(child_space, SPACE::to_virt(m->pfn(order)), SPACE::to_order(order));
  assert (full_flush != child_space->v_lookup(SPACE::to_virt(m->pfn(order))));
  return res;
}

template<>
inline
L4_fpage::Rights
v_delete<Obj_space>(Kobject_mapdb::Mapping *m, int, L4_fpage::Rights flush_rights,
                    bool /*full_flush*/, Auto_tlb_flush<Obj_space> &)
{
  Obj_space::Entry *c = static_cast<Obj_space::Entry*>(m);

  if (c->valid())
    {
      if (flush_rights & L4_fpage::Rights::CR())
        c->invalidate();
      else
        c->del_rights(flush_rights & L4_fpage::Rights::CWS());
    }
  return L4_fpage::Rights(0);
}

template< typename SIZE_TYPE >
static typename SIZE_TYPE::Order_type
get_order_from_fp(L4_fpage const &fp, int base_order = 0)
{
  typedef cxx::underlying_type_t<SIZE_TYPE> Value;
  typedef typename SIZE_TYPE::Order_type Order;

  enum : int {
    Bits = sizeof(Value) * 8 - 1,
    Max  = cxx::is_signed_v<Value> ? Bits - 1 : Bits
  };

  int shift = fp.order() - base_order;

  if (shift <= Max)
    return Order(shift);
  else
    return Order(Max);
}

template<typename Addr, typename Size>
static inline
void free_constraint(Addr &snd_addr, Size &snd_size,
                     Addr &rcv_addr, Size rcv_size,
                     Addr const &hot_spot)
{
  if (rcv_size >= snd_size)
    rcv_addr += cxx::mask_lsb(cxx::get_lsb(hot_spot, rcv_size), snd_size);
  else
    {
      snd_addr += cxx::mask_lsb(cxx::get_lsb(hot_spot, snd_size), rcv_size);
      snd_size = rcv_size;
      // reduce size of address range
    }
}

}

template< typename SPACE >
class Map_traits
{
public:
  static bool match(L4_fpage const &from, L4_fpage const &to);
  static void free_object(typename SPACE::Phys_addr o,
                          typename SPACE::Reap_list **reap_list);
};



//------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>

#include "config.h"
#include "context.h"
#include "kobject.h"
#include "paging.h"
#include "warn.h"


IMPLEMENT template<typename SPACE>
inline
bool
Map_traits<SPACE>::match(L4_fpage const &, L4_fpage const &)
{ return false; }

IMPLEMENT template<typename SPACE>
inline
void
Map_traits<SPACE>::free_object(typename SPACE::Phys_addr,
                               typename SPACE::Reap_list **)
{}


PUBLIC template< typename SPACE >
static inline
typename SPACE::Attr
Map_traits<SPACE>::apply_attribs(typename SPACE::Attr attribs,
                                 typename SPACE::Phys_addr &,
                                 typename SPACE::Attr set_attr)
{ return attribs.apply(set_attr); }


//-------------------------------------------------------------------------
IMPLEMENTATION [io]:

IMPLEMENT template<>
inline
bool
Map_traits<Io_space>::match(L4_fpage const &from, L4_fpage const &to)
{ return from.is_iopage() && (to.is_iopage() || to.is_all_spaces()); }


//-------------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT template<>
inline
bool
Map_traits<Mem_space>::match(L4_fpage const &from, L4_fpage const &to)
{
  return from.is_mempage() && (to.is_all_spaces() || to.is_mempage());
}


IMPLEMENT template<>
inline
bool
Map_traits<Obj_space>::match(L4_fpage const &from, L4_fpage const &to)
{ return from.is_objpage() && (to.is_objpage() || to.is_all_spaces()); }

IMPLEMENT template<>
inline
void
Map_traits<Obj_space>::free_object(Obj_space::Phys_addr o,
                                   Obj_space::Reap_list **reap_list)
{
  if (o->map_root()->no_mappings())
    o->initiate_deletion(reap_list);
}

IMPLEMENT template<>
static inline
Obj_space::Attr
Map_traits<Obj_space>::apply_attribs(Obj_space::Attr attribs,
                                     Obj_space::Phys_addr &a,
                                     Obj_space::Attr set_attr)
{
  if (attribs.extra() & ~set_attr.extra())
    a = a->downgrade(~set_attr.extra());

  attribs &= set_attr;
  return attribs;
}


/**
 * Flexpage mapping.
 *
 * \param from     Source address space
 * \param fp_from  Flexpage descriptor for virtual-address space range in source
 *                 address space
 * \param to       Destination address space
 * \param fp_to    Flexpage descriptor for virtual-address space range in
 *                 destination address space
 * \param control  Message item describing the mapping operation.
 * \param r        List of Kobjects that may be deleted during that operation.

 * \return IPC error
 *
 * This function diverts to mem_map (for memory fpages), io_map (for IO fpages)
 * or obj_map (for capability fpages).
*/
// Don't inline -- it eats too much stack.
// inline NEEDS ["config.h", io_map]
L4_error
fpage_map(Space *from, L4_fpage fp_from, Space *to,
          L4_fpage fp_to, L4_msg_item control, Kobject::Reap_list *r)
{
  Space::Caps caps = from->caps() & to->caps();

  if (Map_traits<Mem_space>::match(fp_from, fp_to) && (caps & Space::Caps::mem()))
    return mem_map(from, fp_from, to, fp_to, control);

#ifdef CONFIG_PF_PC
  if (Map_traits<Io_space>::match(fp_from, fp_to) && (caps & Space::Caps::io()))
    return io_map(from, fp_from, to, fp_to, control);
#endif

  if (Map_traits<Obj_space>::match(fp_from, fp_to) && (caps & Space::Caps::obj()))
    return obj_map(from, fp_from, to, fp_to, control, r->list());

  return L4_error::None;
}

/** Flexpage unmapping.
    divert to mem_fpage_unmap (for memory fpages) or
    io_fpage_unmap (for IO fpages)
    @param space address space that should be flushed
    @param fp    flexpage descriptor of address-space range that should
                 be flushed
    @param me_too If false, only flush recursive mappings.  If true,
                 additionally flush the region in the given address space.
    @param flush_mode determines which access privileges to remove.
    @return combined (bit-ORed) access status of unmapped physical pages
*/
// Don't inline -- it eats too much stack.
// inline NEEDS ["config.h", io_fpage_unmap]
L4_fpage::Rights
fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask, Kobject ***rl)
{
  L4_fpage::Rights ret(0);
  Space::Caps caps = space->caps();

  if ((caps & Space::Caps::io()) && (fp.is_iopage() || fp.is_all_spaces()))
    ret |= io_fpage_unmap(space, fp, mask);

  if ((caps & Space::Caps::obj()) && (fp.is_objpage() || fp.is_all_spaces()))
    ret |= obj_fpage_unmap(space, fp, mask, rl);

  if ((caps & Space::Caps::mem()) && (fp.is_mempage() || fp.is_all_spaces()))
    ret |= mem_fpage_unmap(space, fp, mask);

  return ret;
}


//////////////////////////////////////////////////////////////////////
//
// Utility functions for all address-space types
//

inline
template <typename SPACE >
bool
map_lookup_src(SPACE* from,
               typename SPACE::V_pfn const &snd_addr,
               typename SPACE::Phys_addr *s_phys,
               typename SPACE::Phys_addr *i_phys,
               typename SPACE::Page_order *s_order,
               typename SPACE::Attr *i_attribs,
               typename SPACE::Attr attribs)
{
  typedef Map_traits<SPACE> Mt;
  // Sigma0 special case: Sigma0 doesn't need to have a
  // fully-constructed page table, and it can fabricate mappings
  // for all physical addresses.
  typename SPACE::Attr s_attribs;
  if (EXPECT_FALSE(! from->v_fabricate(snd_addr, s_phys,
                                       s_order, &s_attribs)))
    return false;

  // Compute attributes for to-be-inserted frame
  typename SPACE::V_pfc page_offset = SPACE::subpage_offset(snd_addr, *s_order);
  *i_phys = SPACE::subpage_address(*s_phys, page_offset);

  *i_attribs = Mt::apply_attribs(s_attribs, *i_phys, attribs);
  if (EXPECT_FALSE(i_attribs->empty()))
    return false;

  return true;
}

inline
template <typename SPACE, typename MAPDB> inline
L4_error
map(MAPDB* mapdb,
    SPACE* from, Space *from_id,
    typename SPACE::V_pfn snd_addr,
    typename SPACE::V_pfc snd_size,
    SPACE* to, Space *to_id,
    typename SPACE::V_pfn rcv_addr,
    bool grant, typename SPACE::Attr attribs,
    Mu::Auto_tlb_flush<SPACE> &tlb,
    typename SPACE::Reap_list **reap_list = 0)
{
  using namespace Mu;

  typedef typename SPACE::Attr Attr;
  typedef typename SPACE::Page_order Page_order;

  typedef typename SPACE::V_pfn V_pfn;
  typedef typename SPACE::V_pfc V_pfc;

  typedef typename MAPDB::Frame Frame;

  L4_error condition = L4_error::None;

  bool no_page_mapped = true;

  V_pfn const rcv_start = rcv_addr;
  V_pfc const rcv_size = snd_size;

  auto const &to_fit_size = to->fitting_sizes();

  // We now loop through all the pages we want to send from the
  // sender's address space, looking up appropriate parent mappings in
  // the mapping data base, and entering a child mapping and a page
  // table entry for the receiver.

  // Special care is taken for 4MB page table entries we find in the
  // sender's address space: If what we will create in the receiver is
  // not a 4MB-mapping, too, we have to find the correct parent
  // mapping for the new mapping database entry: This is the sigma0
  // mapping for all addresses != the 4MB page base address.

  // When overmapping an existing page, flush the interfering
  // physical page in the receiver, even if it is larger than the
  // mapped page.

  // verify sender and receiver virtual addresses are still within
  // bounds; if not, bail out.  Sigma0 may send from any address (even
  // from an out-of-bound one)

  // increment variable for our map loop
  V_pfc size;

  V_pfn const to_max = to->map_max_address();
  V_pfn const from_max = from->map_max_address();

  for (;
       snd_size != V_pfc(0)       // pages left for sending?
       && rcv_addr < to_max
       && snd_addr < from_max;

       rcv_addr += size,
       snd_addr += size,
       snd_size -= size)
    {
      // First, look up the page table entries in the sender and
      // receiver address spaces.

      // Sender lookup.
      typename SPACE::Phys_addr s_phys, i_phys;
      Page_order s_order;
      Attr i_attribs;

      // Sigma0 special case: Sigma0 doesn't need to have a
      // fully-constructed page table, and it can fabricate mappings
      // for all physical addresses.
      if (EXPECT_FALSE(! map_lookup_src(from, snd_addr, &s_phys, &i_phys,
                                        &s_order, &i_attribs, attribs)))
        {
          size = SPACE::to_size(s_order) - SPACE::subpage_offset(snd_addr, s_order);
          if (size >= snd_size)
            break;
          continue;
        }

      // We have a mapping in the sender's address space.
      no_page_mapped = false;

      // Receiver lookup.

      // The may be used uninitialized warning for this variable is bogus
      // the v_lookup function must initialize the value if it returns true.
      typename SPACE::Phys_addr r_phys;
      Page_order r_order;
      Attr r_attribs;

      Page_order i_order = to_fit_size(s_order);
      size = SPACE::to_size(i_order);
      bool const rcv_page_mapped = to->v_lookup(rcv_addr, &r_phys, &r_order, &r_attribs);

      while (size > snd_size
          // want to send less than a superpage?
          || i_order > r_order         // not enough space for superpage map?
          || SPACE::subpage_offset(snd_addr, i_order) != V_pfc(0) // snd page not aligned?
          || SPACE::subpage_offset(rcv_addr, i_order) != V_pfc(0) // rcv page not aligned?
          || (rcv_addr + size > rcv_start + rcv_size))
        // rcv area to small?
        {
          i_order = to_fit_size(--i_order);
          size = SPACE::to_size(i_order);
          if (grant)
            {
              WARN("XXX Can't GRANT page from superpage (%p: " L4_PTR_FMT
                  " -> %p: " L4_PTR_FMT "), demoting to MAP\n",
                  static_cast<void *>(from_id),
                  static_cast<unsigned long>(cxx::int_value<V_pfn>(snd_addr)),
                  static_cast<void *>(to_id),
                  static_cast<unsigned long>(cxx::int_value<V_pfn>(rcv_addr)));
              grant = 0;
            }
        }

      if (!mapdb->valid_address(SPACE::to_pfn(s_phys)))
        continue; // no valid sender mapping, skip

      Frame sender_frame;

      if (rcv_page_mapped)
        {
          Frame rcv_frame;
          int r = mapdb->lookup_src_dst(from_id, SPACE::to_pfn(s_phys),
                                        SPACE::to_pfn(SPACE::page_address(snd_addr, s_order)),
                                        to_id, SPACE::to_pfn(r_phys), SPACE::to_pfn(rcv_addr),
                                        &sender_frame, &rcv_frame);

          if (r < 0)
            // nothing found
            continue;

          if (r > 0
              || grant
              || SPACE::page_address(r_phys, i_order) != i_phys
              || r_order > i_order)
            {
              // unmap dst
              auto addr = SPACE::page_address(rcv_addr, r_order);
              auto size = SPACE::to_size(r_order);
              static_cast<SPACE *>(to_id)->v_delete(addr, r_order,
                                                    L4_fpage::Rights::FULL());

              tlb.add_page(to_id, addr, r_order);

              MAPDB::foreach_mapping(rcv_frame, SPACE::to_pfn(addr), SPACE::to_pfn(addr + size),
                  [&tlb](typename MAPDB::Mapping *m, typename MAPDB::Order size)
                  {
                    v_delete<SPACE>(m, size, L4_fpage::Rights::FULL(), true, tlb);
                  });

              mapdb->flush(rcv_frame, L4_map_mask::full(), SPACE::to_pfn(addr),
                           SPACE::to_pfn(addr + size));
              Map_traits<SPACE>::free_object(r_phys, reap_list);

              // Dst is equal to or an ancestor of src.
              if (r == 2)
                {
                  // If dst (rcv_frame) is equal to src (sender_frame), we have
                  // to unlock the rcv_frame here (and thereby also the
                  // sender_frame, as they are the same and therefore use the
                  // same lock). For the case that dst is an ancestor of src,
                  // the sender_frame is not locked by lookup_src_dst().
                  assert(   rcv_frame.same_lock(sender_frame)
                         || sender_frame.frame == nullptr /* i.e. not locked */);
                  rcv_frame.clear();

                  // src is gone too
                  continue;
                }

              // unlock destination if it is not a grant is the same tree
              if (!rcv_frame.same_lock(sender_frame))
                rcv_frame.clear();
            }
          else if (r == 0)
            {
              i_attribs |= r_attribs;
              // we might unlock the sender mapping as we are going to manipulate
              // the existing receiver mapping without doing a mapdb->insert later.
              if (!rcv_frame.same_lock(sender_frame))
                sender_frame.clear();

              // store the still locked rcv mapping for later unlock
              sender_frame = rcv_frame;
            }
        }
      else if (! mapdb->lookup(from_id,
                               SPACE::to_pfn(SPACE::page_address(snd_addr, s_order)),
                               SPACE::to_pfn(s_phys),
                               &sender_frame))
        continue;		// someone deleted this mapping in the meantime



      // from here mapdb_frame is always initialized, so ignore the warning
      // in grant / insert

      // At this point, we have a lookup for the sender frame (s_phys,
      // s_size, s_attribs), the max. size of the receiver frame
      // (r_phys), the sender_mapping, and whether a receiver mapping
      // already exists (doing_upgrade).

      // Do the actual insertion.
      typename SPACE::Status status
        = to->v_insert(i_phys, rcv_addr, i_order, i_attribs);

      switch (status)
        {
        case SPACE::Insert_warn_exists:
        case SPACE::Insert_warn_attrib_upgrade:
        case SPACE::Insert_ok:
          {
            if (grant)
              {
                if (EXPECT_FALSE(!mapdb->grant(sender_frame, to_id,
                                               SPACE::to_pfn(rcv_addr))))
                  {
                    // Error -- remove mapping again.
                    to->v_delete(rcv_addr, i_order, L4_fpage::Rights::FULL());
                    tlb.add_page(to, rcv_addr, i_order);

                    // may fail due to quota limits
                    condition = L4_error::Map_failed;
                    break;
                  }

                from->v_delete(SPACE::page_address(snd_addr, s_order), s_order,
                               L4_fpage::Rights::FULL());
                tlb.add_page(from, SPACE::page_address(snd_addr, s_order), s_order);
                Map_traits<SPACE>::free_object(s_phys, reap_list);
              }
            else if (status == SPACE::Insert_ok)
              {
                if (!mapdb->insert(sender_frame,
                                   to_id, SPACE::to_pfn(rcv_addr),
                                   SPACE::to_pfn(i_phys), SPACE::to_pcnt(i_order)))
                  {
                    // Error -- remove mapping again.
                    to->v_delete(rcv_addr, i_order, L4_fpage::Rights::FULL());
                    tlb.add_page(to, rcv_addr, i_order);

                    // XXX This is not race-free as the mapping could have
                    // been used in the mean-time, but we do not care.
                    condition = L4_error::Map_failed;
                    break;
                  }
              }

            if (SPACE::Need_insert_tlb_flush)
              tlb.add_page(to, rcv_addr, i_order);

            break;
          }

        case SPACE::Insert_err_nomem:
          condition = L4_error::Map_failed;
          break;

        case SPACE::Insert_err_exists:
          WARN("map (%s) skipping area (%p): " L4_PTR_FMT
               " -> %p: " L4_PTR_FMT "(%lx)", SPACE::name,
               static_cast<void *>(from_id),
               static_cast<unsigned long>(cxx::int_value<V_pfn>(snd_addr)),
               static_cast<void *>(to_id),
               static_cast<unsigned long>(cxx::int_value<V_pfn>(rcv_addr)),
               static_cast<unsigned long>(cxx::int_value<V_pfc>(size)));
          // Do not flag an error here -- because according to L4
          // semantics, it isn't.
          break;
        }

      sender_frame.might_clear();

      if (!condition.ok())
        break;
    }

  // FIXME: make this debugging code optional
  if (EXPECT_FALSE(no_page_mapped))
    WARN("nothing mapped: (%s) from [%p]: " L4_PTR_FMT
         " size: " L4_PTR_FMT " to [%p]\n", SPACE::name,
         static_cast<void *>(from_id),
         static_cast<unsigned long>(cxx::int_value<V_pfn>(snd_addr)),
         static_cast<unsigned long>(cxx::int_value<V_pfc>(rcv_size)),
         static_cast<void *>(to_id));

  if (condition.ok() && no_page_mapped)
    condition.set_empty_map();
  return condition;
}

// save access rights for Mem_space
inline template<typename MAPDB>
void
save_access_flags(Mem_space *space, typename Mem_space::V_pfn page_address,
                  bool /* me_too */,
                  typename MAPDB::Frame const& /* mapdb_frame */,
                  L4_fpage::Rights page_rights)
{
  if (L4_fpage::Rights accessed = page_rights & (L4_fpage::Rights::RW()))
    {
      space->v_set_access_flags(page_address, accessed);

      // we have no back reference to our parent, so
      // we cannot store the access rights there in
      // the me_too case...
    }
}

// do nothing for IO and OBJs
template<typename MAPDB, typename SPACE,
         typename = cxx::enable_if_t<!cxx::is_same_v<SPACE, Mem_space>>>
void
save_access_flags(SPACE *, typename SPACE::V_pfn, bool,
                  typename MAPDB::Frame const &,
                  L4_fpage::Rights)
{}


template <typename SPACE, typename MAPDB> inline
L4_fpage::Rights
unmap(MAPDB* mapdb, SPACE* space, Space *space_id,
      typename SPACE::V_pfn start,
      typename SPACE::V_pfc size,
      L4_fpage::Rights rights,
      L4_map_mask mask,
      Mu::Auto_tlb_flush<SPACE> &tlb,
      typename SPACE::Reap_list **reap_list)
{
  using namespace Mu;

  typedef typename MAPDB::Frame Frame;

  typedef typename SPACE::V_pfn V_pfn;
  typedef typename SPACE::V_pfc V_pfc;

  bool me_too = mask.self_unmap();

  L4_fpage::Rights flushed_rights(0);
  V_pfn end = start + size;
  V_pfn const map_max = space->map_max_address();

  typename SPACE::Phys_addr phys;
  V_pfc phys_size;
  V_pfn page_address;

  bool const full_flush = SPACE::is_full_flush(rights);

  // iterate over all pages in "space"'s page table that are mapped
  // into the specified region
  for (V_pfn address = start;
       address < end && address < map_max;
       address = page_address + phys_size)
    {
      // for amd64-mem_space's this will skip the hole in the address space
      address = SPACE::canonize(address);

      bool have_page;

      typename SPACE::Page_order phys_order;
      have_page = space->v_fabricate(address, &phys, &phys_order);

      phys_size = SPACE::to_size(phys_order);
      page_address = SPACE::page_address(address, phys_order);

      // phys_size and page_address have now been set up, allowing the
      // use of continue (which evaluates the for-loop's iteration
      // expression involving these two variables).

      if (! have_page)
        continue;

      if (me_too)
        {
          // Rewind flush address to page address.  We always flush
          // the whole page, even if it is larger than the specified
          // flush area.
          address = page_address;
          if (end < address + phys_size)
            end = address + phys_size;
        }

      // all pages shall be handled by our mapping data base
      assert (mapdb->valid_address(SPACE::to_pfn(phys)));

      Frame mapdb_frame;

      if (! mapdb->lookup(space_id, SPACE::to_pfn(page_address), SPACE::to_pfn(phys),
                          &mapdb_frame))
        // someone else unmapped faster
        continue;		// skip

      L4_fpage::Rights page_rights(0);

      // Delete from this address space
      if (me_too)
        {
          page_rights |=
            space->v_delete(address, phys_order, rights);

          tlb.add_page(space, address, phys_order);
        }

      MAPDB::foreach_mapping(mapdb_frame, SPACE::to_pfn(address), SPACE::to_pfn(end),
          [&page_rights, rights, full_flush, &tlb](typename MAPDB::Mapping *m, typename MAPDB::Order size)
          {
            page_rights |= v_delete<SPACE>(m, size, rights, full_flush, tlb);
          });

      flushed_rights |= page_rights;

      // Store access attributes for later retrieval
      save_access_flags<MAPDB>(space, page_address, me_too, mapdb_frame, page_rights);

      if (full_flush)
        {
          mapdb->flush(mapdb_frame, mask, SPACE::to_pfn(address),
                       SPACE::to_pfn(end));
          Map_traits<SPACE>::free_object(phys, reap_list);
        }

      mapdb_frame.clear();
    }

  return flushed_rights;
}

//----------------------------------------------------------------------------
IMPLEMENTATION[!io || ux]:

// Empty dummy functions when I/O protection is disabled
inline
void init_mapdb_io(Space *)
{}

inline
L4_error
io_map(Space *, L4_fpage const &, Space *, L4_fpage const &, L4_msg_item)
{
  return L4_error::None;
}

inline
L4_fpage::Rights
io_fpage_unmap(Space * /*space*/, L4_fpage const &/*fp*/, L4_map_mask)
{
  return L4_fpage::Rights(0);
}

