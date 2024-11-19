INTERFACE:

#include "kobject_mapdb.h"


IMPLEMENTATION:

#include "assert.h"

#include "obj_space.h"
#include "l4_types.h"

L4_error __attribute__((nonnull(1, 3)))
obj_map(Space *from, L4_fpage const &fp_from,
        Space *to, L4_fpage const &fp_to, L4_snd_item control,
        Kobjects_list &reap_list)
{
  assert(from);
  assert(to);

  Cap_index rcv_addr = fp_to.obj_index();
  Cap_index snd_addr = fp_from.obj_index();
  Order so = Mu::get_order_from_fp<Cap_diff>(fp_from);
  Order ro = Mu::get_order_from_fp<Cap_diff>(fp_to);
  Cap_index offs = Cap_index(control.index());

  snd_addr = cxx::mask_lsb(snd_addr, so);
  rcv_addr = cxx::mask_lsb(rcv_addr, ro);

  Mu::free_constraint(snd_addr, so, rcv_addr, ro, offs);
  Cap_diff snd_size = Cap_diff(1) << so;

  if (snd_size == Cap_diff(0))
    return L4_error::None;

  Obj_space::Attr attribs(fp_from.rights(), L4_snd_item::C_weak_ref ^ control.attr());

  Mu::Auto_tlb_flush<Obj_space> tlb;
  return map<Obj_space>(static_cast<Kobject_mapdb*>(nullptr),
                        from, from, Obj_space::V_pfn(snd_addr),
                        snd_size,
                        to, to, Obj_space::V_pfn(rcv_addr),
                        control.is_grant(), attribs, tlb,
                        reap_list);
}

Page::Flags __attribute__((nonnull(1)))
obj_fpage_unmap(Space * space, L4_fpage fp, L4_map_mask mask,
                Kobjects_list &reap_list)
{
  assert(space);

  Order size = Mu::get_order_from_fp<Cap_diff>(fp);
  Cap_index addr = fp.obj_index();
  addr = cxx::mask_lsb(addr, size);

  // XXX prevent unmaps when a task has no caps enabled

  Mu::Auto_tlb_flush<Obj_space> tlb;

  return unmap<Obj_space>(static_cast<Kobject_mapdb *>(nullptr), space, space,
               Obj_space::V_pfn(addr), Obj_space::V_pfc(1) << size,
               fp.rights(), mask, tlb, reap_list);
}


L4_error __attribute__((nonnull(1, 4)))
obj_map(Space *from, Cap_index snd_addr, unsigned long snd_size,
        Space *to, Cap_index rcv_addr,
        Kobjects_list &reap_list, bool grant = false,
        Obj_space::Attr attribs = Obj_space::Attr::Full())
{
  assert(from);
  assert(to);

  Mu::Auto_tlb_flush<Obj_space> tlb;
  return map<Obj_space>(static_cast<Kobject_mapdb*>(nullptr),
	     from, from, Obj_space::V_pfn(snd_addr),
	     Cap_diff(snd_size),
	     to, to, Obj_space::V_pfn(rcv_addr),
	     grant, attribs, tlb, reap_list);
}

/**
 * Create the first mapping of a kernel object in the given object space and
 * register it with the object mapping database.
 *
 * \pre `o` must not be mapped anywhere yet.
 */
bool __attribute__((nonnull(1, 2, 3)))
map_obj_initially(Kobject_iface *o, Obj_space* to, Space *to_id,
                  Cap_index rcv_addr, Kobjects_list &reap_list)
{
  assert(o);
  assert(to);
  assert(to_id);

  typedef Obj_space SPACE;
  typedef Obj_space::V_pfn Addr;
  typedef Obj_space::Page_order Size;

  if (EXPECT_FALSE(rcv_addr >= to->map_max_address()))
    return 0;

  // Receiver lookup.
  Obj_space::Phys_addr r_phys;
  Size r_size;
  Obj_space::Attr r_attribs;

  Addr ra(rcv_addr);
  Mu::Auto_tlb_flush<Obj_space> tlb;
  if (to->v_lookup(ra, &r_phys, &r_size, &r_attribs))
    unmap(static_cast<Kobject_mapdb*>(nullptr), to, to_id, ra,
          Obj_space::V_pfc(1) << r_size, L4_fpage::Rights::FULL(),
          L4_map_mask::full(), tlb, reap_list);

  // Do the actual insertion.
  Obj_space::Status status = to->v_insert(o, ra, Size(0), Obj_space::Attr::Full());

  switch (status)
    {
    case SPACE::Insert_ok:
      o->map_root()->insert_root_mapping(ra);
      break;

    case SPACE::Insert_err_nomem:
      return 0;

    case SPACE::Insert_err_exists:
      // Do not flag an error here -- because according to L4
      // semantics, it isn't.
    case SPACE::Insert_warn_exists:
    case SPACE::Insert_warn_attrib_upgrade:
      break;
    }

  return 1;
}
