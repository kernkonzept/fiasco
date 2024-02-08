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
      case Msg_bind::Op:
        return Msg_bind::call(this, tag, in, out, rights);

      case Msg_unbind::Op:
        return Msg_unbind::call(this, tag, in , out, rights);

      default:
        return commit_result(-L4_err::ENosys);
      }
  }

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
Dmar::find_mmu(Unsigned64 src_id, bool try_default = false)
{
  Unsigned16 segment = src_id >> 16;
  Unsigned8 bus = src_id >> 8;
  Unsigned8 dev = (src_id >> 3) & 31;
  Unsigned8 fun = src_id & 7;

  for (auto &m: Intel::Io_mmu::iommus)
    {
      if (segment != m.segment)
        continue;

      if (try_default && m.is_default_pci())
        return &m;

      for (auto const &d: m.devs)
        {
          // PCI endpoint
          if (d.type == 1 && d.start_bus_nr == bus
              && d.path[0].dev == dev && d.path[0].func == fun)
            return &m;

          // PCI sub-hierarchy
        }
    }
  return 0;
}

PRIVATE
Mword
Dmar::parse_src_id(Unsigned64 src_id, Unsigned8 *bus, unsigned *dfs,
                   unsigned *dfe, Intel::Io_mmu **mmu)
{
  Unsigned8 type = (src_id >> 18) & 3;
  *bus = src_id >> 8;
  switch (type)
    {
    case 1:
      *dfs = src_id & 0xff;
      *dfe = *dfs + 1;
      src_id = (Unsigned64{*bus} << 8) | *dfs;
      break;
    case 2:
      *dfs = 0;
      *dfe = 0x100;
      src_id = Unsigned64{*bus} << 8;
      break;
    default:
      return -L4_err::EInval;
    }

  *mmu = find_mmu(src_id & 0xffff);
  if (!*mmu)
    *mmu = find_mmu(src_id & 0xffff, true);
  return *mmu ? 0 : -L4_err::EInval;
}

PRIVATE
L4_msg_tag
Dmar::op_bind(Ko::Rights, Unsigned64 src_id, Ko::Cap<Dmar_space> space_cap)
{
  Unsigned8 bus;
  unsigned dfs, dfe;
  Intel::Io_mmu *mmu;
  if (Mword err = parse_src_id(src_id, &bus, &dfs, &dfe, &mmu))
    return Kobject_iface::commit_result(err);

  // Prevent the Dmar_space from being deleted while using it without holding
  // the CPU lock.
  Ref_ptr<Dmar_space> space(space_cap.obj);

  unsigned long did = space->get_did();
  // no free domain id left
  if (EXPECT_FALSE(did == ~0UL))
    return Kobject_iface::commit_result(-L4_err::ENomem);

  auto aw = mmu->aw();
  auto slptptr = space->get_root(aw);

  // Release CPU lock because when binding a Dmar_space, it may be necessary
  // to execute one or more invalidation descriptors via the IOMMU invalidation
  // queue. While waiting for these to execute, we must be interruptible.
  auto guard_cpu = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  bool need_wait = false;
  for (unsigned df = dfs; df < dfe; ++df)
    {
      auto entryp = mmu->get_context_entry(bus, df, true);

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

      need_wait |= mmu->set_context_entry(entryp, bus, df, entry);
    }
  if (need_wait)
    mmu->queue_and_wait();

  return Kobject_iface::commit_result(0);
}

PRIVATE
L4_msg_tag
Dmar::op_unbind(Ko::Rights, Unsigned64 src_id, Ko::Cap<Dmar_space> space_cap)
{
  Unsigned8 bus;
  unsigned dfs, dfe;
  Intel::Io_mmu *mmu;
  if (Mword err = parse_src_id(src_id, &bus, &dfs, &dfe, &mmu))
    return Kobject_iface::commit_result(err);

  // Prevent the Dmar_space from being deleted while using it without holding
  // the CPU lock.
  Ref_ptr<Dmar_space> space(space_cap.obj);

  auto slptptr = space->get_root(mmu->aw());

  // Release CPU lock because when unbinding a Dmar_space, it may be necessary
  // to execute one or more invalidation descriptors via the IOMMU invalidation
  // queue. While waiting for these to execute, we must be interruptible.
  auto guard_cpu = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  bool need_wait = false;
  for (unsigned df = dfs; df < dfe; ++df)
    {
      auto entryp = mmu->get_context_entry(bus, df, false);
      // nothing bound, skip
      if (!entryp)
        continue;

      Intel::Io_mmu::Cte entry = access_once(entryp.unsafe_ptr());
      // different space bound, skip
      if (entry.slptptr() != slptptr)
        continue;

      // when the CAS fails someone else already unbound this slot,
      // so ignore that case
      mmu->cas_context_entry(entryp, bus, df, entry, Intel::Io_mmu::Cte(),
                             &need_wait);
    }
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

