IMPLEMENTATION [iommu]:

#include "kobject_helper.h"
#include "dmar_space.h"
#include "intel_iommu.h"
#include "mem_unit.h"
#include "warn.h"

#include <cstdio>

JDB_DEFINE_TYPENAME(Dmar, "IOMMU");

struct Dmar : Kobject_h<Dmar, Kobject>
{
  L4_RPC(0, bind,   (Unsigned64 src_id, Ko::Cap<Dmar_space> dma_space));
  L4_RPC(1, unbind, (Unsigned64 src_id, Ko::Cap<Dmar_space> dma_space));

  L4_msg_tag kinvoke(L4_obj_ref, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb const *in, Utcb *out)
  {
    L4_msg_tag tag = f->tag();

    if (!Ko::check_basics(&tag, rights, L4_msg_tag::Label_iommu,
                          L4_fpage::Rights::CS()))
      return tag;

    switch (access_once(&in->values[0]))
      {
      case Msg_bind::Opcode:
        return Msg_bind::call(this, tag, in, out, rights);

      case Msg_unbind::Opcode:
        return Msg_unbind::call(this, tag, in , out, rights);

      default:
        return commit_result(-L4_err::ENosys);
      }
  }

  struct Src_id
  {
    Unsigned64 raw = 0;
    explicit Src_id(Unsigned64 id) : raw(id) {}

    CXX_BITFIELD_MEMBER( 0,  7, devfn, raw);
    CXX_BITFIELD_MEMBER( 0,  2, fn, raw);
    CXX_BITFIELD_MEMBER( 3,  7, dev, raw);
    CXX_BITFIELD_MEMBER( 8, 15, bus, raw);
    CXX_BITFIELD_MEMBER(18, 19, type, raw);
    CXX_BITFIELD_MEMBER(32, 47, iommu_idx, raw);
  };
};


PRIVATE static
void
Dmar::enable_dmar(Intel::Io_mmu *mmu, bool passthrough)
{
  typedef Intel::Io_mmu::Cte Cte;

  bool const has_pass = mmu->ecaps & (1 << 6);
  unsigned const aw = mmu->aw();
  mmu->allocate_root_table();

  if (passthrough)
    {
      Unsigned64 identity_root = 0;
      if (!has_pass)
        {
          Dmar_space::create_identity_map();
          identity_root = Dmar_space::get_root(Dmar_space::identity_map, aw);
        }

      for (unsigned bus = 0; bus < 256; ++bus)
        {
          Cte::Ptr ct = mmu->get_context_entry(bus, 0, true);
          assert (ct);
          for (unsigned df = 0; df < 256; ++df)
            {
              // we can use unsafe pointer here as this is used with
              // a disabled IOMMU anyways
              Cte *entryp = ct.unsafe_ptr() + df;
              assert(entryp && !entryp->present() && !entryp->os());

              Cte n = Cte();

              if (has_pass)
                {
                  n.tt() = Cte::Tt_passthrough;
                  n.aw() = aw;
                  n.present() = 1;
                }
              else
                {
                  n.tt() = Cte::Tt_translate_all;
                  n.aw() = aw;
                  n.did() = 1;
                  n.slptptr() = identity_root;
                  n.present() = 1;
                }

              *entryp = n;
            }
        }
    }

  if (!mmu->coherent())
    Mem_unit::clean_dcache();
  mmu->queue_and_wait<false>(Intel::Io_mmu::Inv_desc::cc_full(),
                             Intel::Io_mmu::Inv_desc::iotlb_glbl());
  mmu->modify_cmd(Intel::Io_mmu::Cmd_te);
}

PRIVATE
Intel::Io_mmu *
Dmar::find_mmu(Unsigned16 iommu_idx)
{
  if (iommu_idx >= Intel::Io_mmu::Max_iommus)
    return nullptr;

  Intel::Io_mmu &iommu = Intel::Io_mmu::iommus[iommu_idx];
  return &iommu;
}

PRIVATE
Mword
Dmar::parse_src_id(Src_id src_id, Unsigned8 *bus, unsigned *devfn,
                   Intel::Io_mmu **mmu)
{
  Unsigned8 type = src_id.type();
  *bus = src_id.bus();
  switch (type)
    {
    case 1:
      *devfn = static_cast<unsigned>(src_id.devfn());
      break;
    case 2:
      WARNX(Warning,
            "Bus matching not supported. Please match via requester Id\n");
      [[fallthrough]];
    default:
      return -L4_err::EInval;
    }

  *mmu = find_mmu(src_id.iommu_idx());
  return *mmu ? 0 : -L4_err::EInval;
}

/*
 * L4-IFACE: kernel-iommu.iommu-bind
 * PROTOCOL: L4_PROTO_IOMMU
 */
PRIVATE
L4_msg_tag
Dmar::op_bind(Ko::Rights, Unsigned64 src_id, Ko::Cap<Dmar_space> space_cap)
{
  Unsigned8 bus;
  unsigned devfn;
  Intel::Io_mmu *mmu;
  if (Mword err = parse_src_id(Src_id(src_id), &bus, &devfn, &mmu))
    return Kobject_iface::commit_result(err);

  // Prevent the Dmar_space from being deleted while using it without holding
  // the CPU lock.
  Ref_ptr<Dmar_space> space(space_cap.obj);

  Dmar_space::Did did = space->get_did();
  // no free domain id left
  if (EXPECT_FALSE(did == Dmar_space::Invalid_did))
    return Kobject_iface::commit_result(-L4_err::ENomem);

  auto aw = mmu->aw();
  auto slptptr = space->get_root(aw);

  // Release CPU lock because when binding a Dmar_space, it may be necessary
  // to execute one or more invalidation descriptors via the IOMMU invalidation
  // queue. While waiting for these to execute, we must be interruptible.
  auto guard_cpu = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  auto entryp = mmu->get_context_entry(bus, devfn, true);

  // AW: entryp might be NULL is this a user error?
  // AW: treat it as such
  if (!entryp)
    return Kobject_iface::commit_result(-L4_err::ENomem);

  Intel::Io_mmu::Cte entry;
  entry.tt() = Intel::Io_mmu::Cte::Tt_translate_all;
  entry.slptptr() = slptptr;
  entry.aw() = aw;
  entry.did() = did;
  entry.os() = 1;
  entry.present() = 1;

  if (mmu->set_context_entry(entryp, bus, devfn, entry))
    mmu->queue_and_wait();

  return Kobject_iface::commit_result(0);
}

/*
 * L4-IFACE: kernel-iommu.iommu-unbind
 * PROTOCOL: L4_PROTO_IOMMU
 */
PRIVATE
L4_msg_tag
Dmar::op_unbind(Ko::Rights, Unsigned64 src_id, Ko::Cap<Dmar_space> space_cap)
{
  Unsigned8 bus;
  unsigned devfn;
  Intel::Io_mmu *mmu;
  if (Mword err = parse_src_id(Src_id(src_id), &bus, &devfn, &mmu))
    return Kobject_iface::commit_result(err);

  // Prevent the Dmar_space from being deleted while using it without holding
  // the CPU lock.
  Ref_ptr<Dmar_space> space(space_cap.obj);

  auto slptptr = space->get_root(mmu->aw());

  // Release CPU lock because when unbinding a Dmar_space, it may be necessary
  // to execute one or more invalidation descriptors via the IOMMU invalidation
  // queue. While waiting for these to execute, we must be interruptible.
  auto guard_cpu = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  auto entryp = mmu->get_context_entry(bus, devfn, false);
  // nothing bound
  if (!entryp)
    return Kobject_iface::commit_result(0);

  Intel::Io_mmu::Cte entry = access_once(entryp.unsafe_ptr());
  // different space bound
  if (entry.slptptr() != slptptr)
    return Kobject_iface::commit_result(0);

  bool need_wait = false;
  // when the CAS fails someone else already unbound this slot,
  // so ignore that case
  mmu->cas_context_entry(entryp, bus, devfn, entry, Intel::Io_mmu::Cte(),
                         &need_wait);
  if (need_wait)
    mmu->queue_and_wait();

  return Kobject_iface::commit_result(0);
}

static Static_object<Dmar> _glbl_iommu;

PUBLIC static
void
Dmar::init()
{
  if (!Intel::Io_mmu::iommus.size())
    return;

  unsigned max_did = Dmar_space::Max_nr_did;
  for (auto &mmu: Intel::Io_mmu::iommus)
    if (max_did > mmu.num_domains())
      max_did = mmu.num_domains();

  Dmar_space::init(max_did);

  bool pass = false;
#ifdef CONFIG_IOMMU_PASSTHROUGH
  pass = true;
  printf("IOMMU: DMA passthrough enabled!\n");
#endif

  for (unsigned i = 0; i < Intel::Io_mmu::iommus.size(); ++i)
    {
      enable_dmar(&Intel::Io_mmu::iommus[i], pass);
    }

  _glbl_iommu.construct();
  initial_kobjects->register_obj(_glbl_iommu, Initial_kobjects::Iommu);
}

STATIC_INITIALIZE(Dmar);

