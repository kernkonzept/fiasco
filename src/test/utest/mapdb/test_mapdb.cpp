/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 */

INTERFACE:

static char const __attribute__((unused)) *Mapdb_group = "Mapdb";

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "utest_fw.h"
#include "kmem_alloc.h"
#include "mapdb.h"
#include "ram_quota.h"
#include "space.h"
#include "common_test_mapdb.h"

void
init_unittest()
{
  // As we do output matching, wait until all app CPUs are done booting to
  // prevent concurrent screen output.
  Utest::wait_for_app_cpus();

  Utest_fw::tap_log.start();

  Mapdb_ext_test().test_mapdb_basic<Simple_mapdb>();
  Mapdb_ext_test().test_mapdb_maphole<Simple_mapdb>();
  Mapdb_ext_test().test_mapdb_flushtest<Simple_mapdb>();
  Mapdb_ext_test().test_mapdb_basic<Multilevel_mapdb>();
  Mapdb_ext_test().test_mapdb_maphole<Multilevel_mapdb>();
  Mapdb_ext_test().test_mapdb_flushtest<Multilevel_mapdb>();
  Mapdb_ext_test().test_mapdb_multilevel<Multilevel_mapdb>();

  Utest_fw::tap_log.finish();
}

/**
 * Helper class for managing an array of TLB entries.
 */
template <typename Base>
class Test_tlb : public Base
{
public:
  enum { N_tlb = 12 };

  template <typename ...Args>
  Test_tlb(Args && ...args)
  : Base(cxx::forward<Args>(args)...)
  { memset(tlb, 0, sizeof(tlb)); }

  struct Tlb_entry
  {
    Mapdb::Pfn virt;
    Mapdb::Pfn phys;
    Mapdb::Order order;
    bool valid;
  };

  Tlb_entry tlb[N_tlb];
};

/**
 * Find the matching TLB entry corresponding to a virtual address.
 */
PUBLIC
template <typename Base>
typename Test_tlb<Base>::Tlb_entry *
Test_tlb<Base>::match(Mapdb::Pfn virt)
{
  for (auto &e : tlb)
    {
      if (!e.valid)
        continue;

      if (cxx::mask_lsb(virt, e.order) == e.virt)
        return &e;
    }

  return nullptr;
}

/**
 * Find the matching TLB entry corresponding to a virtual address.
 *
 * \param virt  Virtual address of the TLB entry.
 * \returns the TLB entry matching the virtual address.
 */
PRIVATE
template <typename Base>
typename Test_tlb<Base>::Tlb_entry const *
Test_tlb<Base>::match(Mapdb::Pfn virt) const
{ return const_cast<Test_tlb *>(this)->match(virt); }

/**
 * Clear (invalidate) all TLB entries.
 */
PRIVATE
template <typename Base>
void
Test_tlb<Base>::clear()
{
  for (auto &e : tlb)
    e.valid = false;
}

/**
 * Insert a new TLB entry (a virtual address maps to a physical address).
 *
 * \param virt   Virtual address.
 * \param phys   Physical address.
 * \param order  Order of the mapping (log2 of the size).
 *
 * \retval true TLB entry successfully inserted.
 * \retval false TLB entry table either full or TLB entry already present.
 */
PUBLIC
template <typename Base>
bool
Test_tlb<Base>::insert(Mapdb::Pfn virt, Mapdb::Pfn phys, Mapdb::Order order)
{
  if (match(virt))
    {
      printf("overmap: %s\n", Base::name);
      return false;
    }

  for (auto &e : tlb)
    if (!e.valid)
      {
        if (0)
          {
            unsigned long va =
              cxx::int_value<Mem_space::V_pfn>(Mem_space::to_virt(virt))
                << Virt_addr::Shift;
            printf("PT INSERT: %s pa=%lx:%lx va=%lx:%lx order=%d\n",
                   Base::name,
                   cxx::int_value<Mapdb::Pfn>(phys) << Virt_addr::Shift,
                   cxx::int_value<Mapdb::Pfn>(cxx::mask_lsb(phys, order)),
                   va,
                   cxx::int_value<Mapdb::Pfn>(cxx::mask_lsb(virt, order)),
                   cxx::int_value<Mapdb::Order>(order));
          }
        e.virt = cxx::mask_lsb(virt, order);
        e.phys = cxx::mask_lsb(phys, order);
        e.order = order;
        e.valid = true;
        return true;
      }

  printf("TLB overflow: %s\n", Base::name);
  return false;
}

/**
 * Remove a TLB entry.
 *
 * \param virt  Virtual address of the TLB entry to remove.
 *
 * \retval true TLB entry successfully removed.
 * \retval false There does no TLB entry for that virtual address exist.
 */
PUBLIC
template <typename Base>
bool
Test_tlb<Base>::remove(Mapdb::Pfn virt)
{
  auto *e = match(virt);
  if (!e || !e->valid)
    return false;

  if (0)
    {
      unsigned long va =
        cxx::int_value<Mem_space::V_pfn>(Mem_space::to_virt(e->virt))
          << Virt_addr::Shift;
      printf("PT REMOVE: %s va=0x%lx pa=0x%lx order=%d\n",
             Base::name, va,
             cxx::int_value<Mapdb::Pfn>(e->phys) << Virt_addr::Shift,
             cxx::int_value<Mapdb::Order>(e->order));
    }
  e->valid = false;
  return true;
}

class Mapdb_param
{
public:
  enum
  {
    O_page = Config::PAGE_SHIFT,
    O_super = Config::SUPERPAGE_SHIFT,
    O_1M = 20,
    O_2M,
    O_4M,
    O_1G = 30,
  };
  Mapping::Order parent_page_shift;
  size_t *page_shifts;
  unsigned page_shifts_num;
};

class Simple_mapdb : public Mapdb_param
{
public:
  Simple_mapdb()
  {
    enum { Phys_bits = 32 };
    parent_page_shift = Mapping::Order(Phys_bits - O_page);
    static size_t pz[] = { O_2M - O_page, 0 };
    page_shifts = pz;
    page_shifts_num = sizeof(sizeof(pz) / sizeof(pz[0]));
  }
};

class Multilevel_mapdb : public Mapdb_param
{
public:
  Multilevel_mapdb()
  {
    enum { Phys_bits = 42 };
    parent_page_shift = Mapping::Order(Phys_bits - O_page);
    static size_t pz[] = { O_1G - O_page, O_4M - O_page, O_2M - O_page, 0 };
    page_shifts = pz;
    page_shifts_num = sizeof(sizeof(pz) / sizeof(pz[0]));
  }
};

class Mapdb_ext_test : public Mapdb_test_base
{
  template <typename T> using Ptr = cxx::unique_ptr<T, Utest::Deleter<T>>;
  using Test_space_w_tlb          = Test_tlb<Test_space>;
  using Test_sigma0_space_w_tlb   = Test_tlb<Test_s0_space>;
  using Space_ptr                 = Ptr<Test_space_w_tlb>;
  using Test_s0_ptr               = Ptr<Test_sigma0_space_w_tlb>;
  using Mapdb_ptr                 = Ptr<Mapdb>;

private:
  Mapdb &create_mapdb(Mapdb_param p)
  {
    mapdb = Utest::kmem_create<Mapdb>(&*sigma0, p.parent_page_shift,
                                      p.page_shifts, p.page_shifts_num);
    return *mapdb;
  }

  static inline Mapdb::Pcnt to_pcnt(Mapdb_test_base::Order order)
  { return Mem_space::to_pcnt(Mem_space::Page_order(order)); }

  static inline Mapdb::Order to_po(Mapdb_test_base::Order order)
  {
    UTEST_GE(Utest::Assert, order, O_page, "to_po order too small");
    return Mapdb::Order(order - O_page);
  }

private:
  Test_fake_factory rq;
  Test_s0_ptr sigma0 = Utest::kmem_create<Test_sigma0_space_w_tlb>(&rq);
  Space_ptr other    = Utest::kmem_create<Test_space_w_tlb>(&rq, "other");
  Space_ptr client   = Utest::kmem_create<Test_space_w_tlb>(&rq, "client");
  Test_s0_ptr &grandfather = sigma0;
  Space_ptr father   = Utest::kmem_create<Test_space_w_tlb>(&rq, "father");
  Space_ptr son      = Utest::kmem_create<Test_space_w_tlb>(&rq, "son");
  Space_ptr daughter = Utest::kmem_create<Test_space_w_tlb>(&rq, "daughter");
  Space_ptr aunt     = Utest::kmem_create<Test_space_w_tlb>(&rq, "aunt");
  Mapdb_ptr mapdb;
};

PUBLIC
Mapdb_ext_test::Mapdb_ext_test()
{
  sigma0->insert(to_pfn(_1G * 0), to_pfn(_1G * 0), to_po(O_1G));
  sigma0->insert(to_pfn(_1G * 1), to_pfn(_1G * 1), to_po(O_1G));
  sigma0->insert(to_pfn(_1G * 2), to_pfn(_1G * 2), to_po(O_1G));
  sigma0->insert(to_pfn(_1G * 3), to_pfn(_1G * 3), to_po(O_1G));
}

/**
 * Print information about the node.
 *
 * The output is compared against stored output.
 */
PRIVATE
void
Mapdb_ext_test::print_node(Mapdb::Frame const &frame, Mapping const *sub = 0)
{
  auto node = *frame.m ? frame.m : frame.frame->first();
  int const n_depth = *frame.m ? node->depth() + 1 : 0;
  pr_tag("%u:%s%*sspace=%s vaddr=0x%lx size=0x%lx\n",
         n_depth, *frame.m && *node == sub ? " ==> " : "     ",
         n_depth, "", node_name(frame.pspace()), to_virt(frame.pvaddr()),
         to_virt(Mapdb::Pfn(1) << frame.page_shift()));

  Mapdb::foreach_mapping(frame, Mapdb::Pfn(0), Mapdb::Pfn(~0),
                         [sub](Mapping *node, Mapdb::Order order)
    {
      int const n_depth = node->depth() + 1;
      pr_tag("%u:%s%*sspace=%s vaddr=0x%lx size=0x%lx\n",
             n_depth, node == sub ? " ==> " : "     ",
             n_depth, "", node_name(node->space()), to_virt(node->pfn(order)),
             to_virt((Mapdb::Pfn(1) << order)));
    });
  pr_tag("\n");
}

PRIVATE
void
Mapdb_ext_test::show_tree(Treemap *pages,
                          Mapping::Pcnt offset = Mapping::Pcnt(0),
                          Mdb_types::Order base_size = Mapping::Order(0),
                          int indent = 0)
{
  typedef Treemap::Page Page;
  Page page = pages->trunc_to_page(offset);
  Physframe *f = pages->frame(page);

  if (!f->has_mappings())
    return;

  auto frame_va = offset + pages->page_offset();

  pr_tag("%*smapping tree: { %s va=0x%lx size=0x%lx\n",
         indent, "", node_name(pages->owner()),
         to_virt(frame_va),
         to_virt(Mapdb::Pfn(1) << pages->page_shift()));

  pr_tag("%*sheader info: lock: %d\n", indent + 2, "", f->lock.test());

  int ind = 0;
  for (auto m: *f->tree())
    {
      pr_tag("%*s: ", indent + 2, "");

      if (m->submap())
        printf("subtree...\n");
      else
        {
          int depth = m->depth() + 1;
          ind = depth * 2;
          printf("%*sva=0x%lx space=%s depth=",
                 ind, "", to_virt(pages->vaddr(m)), node_name(m->space()));
          if (depth == 0)
            printf("root\n");
          else
            printf("%d\n", depth);
        }

      if (m->submap())
        for (Mapping::Pcnt subo = Mapping::Pcnt(0);
             cxx::mask_lsb(subo,  pages->page_shift()) == Mapping::Pcnt(0);
             subo += (Mapping::Pcnt(1) << m->submap()->page_shift()))
          show_tree(m->submap(), subo, base_size, indent + 2 + ind);
    }

  pr_tag("%*s} // mapping tree: %s va=0x%lx\n",
         indent, "", node_name(pages->owner()), to_virt(frame_va));
}

PRIVATE
void
Mapdb_ext_test::show_tree_nl(Treemap *pages,
                             Mapping::Pcnt offset = Mapping::Pcnt(0),
                             Mdb_types::Order base_size = Mapping::Order(0),
                             int indent = 0)
{
  show_tree(pages, offset, base_size, indent);
  pr_tag("\n");
}

PRIVATE
void
Mapdb_ext_test::print_whole_tree(Mapdb::Frame const &frame)
{
  auto f = frame;
  f.m = Mapping_tree::Iterator();
  print_node(f);
}

PRIVATE
Mapping *
Mapdb_ext_test::insert(Mapdb &m, Mapdb::Frame const &frame,
                       Test_space_w_tlb *space, Mapdb::Pfn virt,
                       Mapdb::Pfn phys, Mapdb::Order order)
{
  pr_tag("Insert: %s va=0x%lx pa=0x%lx order=%d\n",
         node_name(space), to_virt(virt), to_virt(phys),
         cxx::int_value<Mapdb::Order>(order));

  Mapping *sub = m.insert(frame, space, virt, phys, Mapdb::Pcnt(1) << order);
  if (!sub)
    return nullptr;

  UTEST_TRUE(Utest::Expect, space->insert(virt, phys, order),
             "Insert into space");
  return sub;
}

PRIVATE
template <typename Base>
typename Test_tlb<Base>::Tlb_entry const *
Mapdb_ext_test::lookup(Mapdb &m, Test_tlb<Base> *space, Mapdb::Pfn virt,
                       Mapdb::Frame *frame)
{
  auto e = space->match(virt);
  if (!e)
    {
      if (0)
        printf("page-table lookup: 0x%lx in %s failed\n",
               to_virt(virt), node_name(space));
      return nullptr;
    }

  if (!m.lookup(space, virt, e->phys, frame))
    {
      if (1)
        {
          if (!e->valid)
            printf("MDB lookup: 0x%lx in %s: invalid entry\n",
                   to_virt(virt), node_name(space));
          else
            printf("MDB lookup: 0x%lx in %s: va=0x%lx pa=0x%lx order=%d\n",
                   to_virt(virt), node_name(space),
                   to_virt(e->virt), to_virt(e->phys),
                   cxx::int_value<Mapdb::Order>(e->order));
        }
      return nullptr;
    }

  return e;
}

PRIVATE
template <typename Base>
bool
Mapdb_ext_test::map(Mapdb &m, Test_tlb<Base> *from, Mapdb::Pfn fa,
                    Test_space_w_tlb *to, Mapdb::Pfn ta, Mapdb::Order order)
{
  Mapping *sub;
  Mapdb::Frame f;
  auto e = lookup(m, from, fa, &f);
  if (!e)
    {
      printf("Could not lookup source mapping: %s @ 0x%lx\n",
             node_name(from), to_virt(fa));
      return false;
    }

  if (e->order < order)
    {
      f.clear();
      printf("Could not create mapping (target size too large)\n"
             " from %s @ 0x%lx order %d --> to %s @ 0x%lx order %d\n",
             node_name(from), to_virt(fa), cxx::int_value<Mapdb::Order>(e->order),
             node_name(to), to_virt(ta), cxx::int_value<Mapdb::Order>(order));
      return false;
    }

  fa = cxx::mask_lsb(fa, order);
  if (cxx::get_lsb(ta, order) != Mapdb::Pcnt(0))
    {
      f.clear();
      printf("Could not create mapping (misaligned)\n"
             " from %s @ 0x%lx order %d --> to %s @ 0x%lx order %d\n",
             node_name(from), to_virt(fa), cxx::int_value<Mapdb::Order>(e->order),
             node_name(to), to_virt(ta), cxx::int_value<Mapdb::Order>(order));
      return false;
    }


  if (!(sub = insert(m, f, to, ta, e->phys | cxx::get_lsb(fa, e->order), order)))
    {
      f.clear();
      printf("Could not create mapping (MDB insert failed)\n"
             " from %s @ 0x%lx order %d --> to %s @ 0x%lx order %d\n",
             node_name(from), to_virt(fa), cxx::int_value<Mapdb::Order>(e->order),
             node_name(to), to_virt(ta), cxx::int_value<Mapdb::Order>(order));
      return false;
    }

  print_node(f, sub);
  f.clear();
  return true;
}

PRIVATE
template <typename Base>
bool
Mapdb_ext_test::unmap(Mapdb &m, Test_tlb<Base> *space, Mapdb::Pfn va_start,
                      Mapdb::Pfn va_end, bool me_too)
{
  Mapdb::Frame f;
  auto e = lookup(m, space, va_start, &f);
  if (!e)
    {
      printf("Could not lookup mapping: %s @ 0x%lx\n",
             node_name(space), to_virt(va_start));
      return false;
    }

  if (   cxx::mask_lsb(va_start, e->order)
      != cxx::mask_lsb(va_end - Mapdb::Pcnt(1), e->order))
    {
      printf("va range (0x%lx-0x%lx) must be in the original page (order=%d)\n",
             to_virt(va_start), to_virt(va_end), cxx::int_value<Mapdb::Order>(e->order));
      return false;
    }

  if (me_too)
    space->remove(va_start);

  // now delete from the other address spaces
  Mapdb::foreach_mapping(f, va_start, va_end, [](Mapping *m, Mapdb::Order size)
    {
      Test_space_w_tlb *s = static_cast<Test_space_w_tlb *>(m->space());
      Mapdb::Pfn page = m->pfn(size);
      pr_tag("unmap %s va=0x%lx for node:\n", node_name(s), to_virt(page));
      s->remove(page);
    });

  m.flush(f, me_too ? L4_map_mask::full() : L4_map_mask(0), va_start, va_end);
  f.clear();
  pr_tag("state after flush\n");
  return true;
}

PUBLIC
template <typename M>
void
Mapdb_ext_test::test_mapdb_basic()
{
  Utest_fw::tap_log.new_test(Mapdb_group, __func__,
                             "4d74adda-6b14-4042-a441-761d78a89865");

  Mapdb &m = create_mapdb(M());
  Mapping *sub;
  Mapdb::Frame f;

  UTEST_FALSE(Utest::Expect,
              m.lookup(&*other, to_pfn(S_page), to_pfn(S_page), &f),
              "Lookup @other at phys=page");
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*sigma0, to_pfn(0), to_pfn(0), &f),
             "Lookup @sigma0 at phys=0");
  pr_tag("Lookup @sigma0 node at phys=0\n");
  print_node(f);

  UTEST_TRUE(Utest::Assert,
             (sub = m.insert(f, &*other, to_pfn(2 * S_page),
                             to_pfn(S_page), to_pcnt(O_page))),
           "Insert sub-mapping @other");
  pr_tag("Lookup @sigma0 node at phys=0 after inserting sub-mapping\n");
  print_node(f, sub);
  f.clear();

  UTEST_TRUE(Utest::Expect,
             m.lookup(&*sigma0, to_pfn(2 * _2M), to_pfn(2 * _2M), &f),
             "Lookup @sigma0 at phys=2*2M");
  pr_tag("Lookup @sigma0 at phys=2*2M\n");
  print_node(f, sub);

  UTEST_TRUE(Utest::Assert,
             (sub = m.insert(f, &*other, to_pfn(4 * _2M),
                             to_pfn(2 * _2M), to_pcnt(O_2M))),
           "Insert sub-mapping @other");

  pr_tag("Lookup @sigma0 at phys=2*superpage after inserting sub-mapping\n");
  print_node(f, sub);

  auto parent_ma = f.m;

  f.clear();
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*other, to_pfn(4 * _2M), to_pfn(2 * _2M), &f),
             "Lookup @other at phys=4*superpage");
  pr_tag("Lookup @other at phys=4*superpage\n");
  print_node(f);

  UTEST_EQ(Utest::Expect,
           f.treemap->page_shift(), Mapdb::Order(O_2M - O_page),
           "Page order equal");

  UTEST_TRUE(Utest::Assert,
             m.insert(f, &*client, to_pfn(15 * S_page),
                      to_pfn(2 * S_super), to_pcnt(O_page)),
             "Insert 4K sub-mapping");

  f.m = parent_ma;
  pr_tag("Lookup @sigma0 node at phys=2*superpage after inserting sub-mapping\n");
  print_node(f);

  f.clear();
}

/**
 * Removing a child has no influences to the siblings of that child.
 */
PUBLIC
template <typename M>
void
Mapdb_ext_test::test_mapdb_maphole()
{
  Utest_fw::tap_log.new_test(Mapdb_group, __func__,
                             "ff389263-d3c8-4775-b275-2da4560d6eac");

  Mapdb &m = create_mapdb(M());
  Mapdb::Frame f;

  pr_tag("Lookup @grandfather phys=0\n");
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*grandfather, to_pfn(0), to_pfn(0), &f),
             "Lookup @grandfather at phys=0");
  print_whole_tree(f);

  pr_tag("Insert father mapping\n");
  UTEST_TRUE(Utest::Expect,
             m.insert(f, &*father, to_pfn(0), to_pfn(0), to_pcnt(O_page)),
             "Insert mapping @father at phys=0");
  print_whole_tree(f);
  f.clear();

  // GF -> F

  pr_tag("Lookup @father at phys=0\n");
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*father, to_pfn(0), to_pfn(0), &f),
             "Lookup @grandfather at phys=0");
  print_whole_tree(f);

  pr_tag("Insert son mapping\n");
  UTEST_TRUE(Utest::Expect,
             m.insert(f, &*son, to_pfn(0), to_pfn(0), to_pcnt(O_page)),
             "Insert mapping @son at phys=0");
  print_whole_tree(f);
  f.clear();

  // GF -> F -> S

  pr_tag("Lookup @father at phys=0\n");
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*father, to_pfn(0), to_pfn(0), &f),
             "Lookup @father at phys=0");
  print_whole_tree(f);

  pr_tag("Insert daughter mapping\n");
  UTEST_TRUE(Utest::Expect,
             m.insert(f, &*daughter, to_pfn(0), to_pfn(0), to_pcnt(O_page)),
             "Insert mapping @daughter at phys=0");
  print_whole_tree(f);
  f.clear();

  // GF -> F -> S
  //         -> D

  pr_tag("Lookup @son at phys=0\n");
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*son, to_pfn(0), to_pfn(0), &f),
             "Lookup @son at phys=0");
  print_whole_tree(f);
  show_tree_nl(m.dbg_treemap());

  pr_tag("Son has accident on return from disco\n");
  m.flush(f, L4_map_mask::full(), to_pfn(0), to_pfn(S_page));
  f.clear();
  show_tree_nl(m.dbg_treemap());

  // GF -> F -> D

  pr_tag("Lost aunt returns from holiday\n");
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*grandfather, to_pfn(0), to_pfn(0), &f),
             "Lookup @grandfather at phys=0");
  print_whole_tree(f);

  pr_tag("Insert @aunt mapping\n");
  UTEST_TRUE(Utest::Expect,
             m.insert(f, &*aunt, to_pfn(0), to_pfn(0), to_pcnt(O_page)),
             "Insert mapping @aunt at phys=0");
  print_whole_tree(f);
  f.clear();
  show_tree_nl(m.dbg_treemap());

  // GF -> F -> D
  //    -> A

  pr_tag("Lookup @daughter at phys=0\n");
  UTEST_TRUE(Utest::Expect,
             m.lookup(&*daughter, to_pfn(0), to_pfn(0), &f),
             "Lookup @daughter at phys=0");
  print_whole_tree(f);
  f.clear();
}

/**
 * Removing a child node also removes the child node of that child.
 */
PUBLIC
template <typename M>
void
Mapdb_ext_test::test_mapdb_flushtest()
{
  Utest_fw::tap_log.new_test(Mapdb_group, __func__,
                             "fffeb9bb-f259-4cad-b4b5-4a34ba27b4db");

  Mapdb &m = create_mapdb(M());
  Mapdb::Frame f;

  pr_tag("Lookup @grandfather\n");
  UTEST_TRUE(Utest::Expect, m.lookup(&*grandfather, to_pfn(0), to_pfn(0), &f),
             "Lookup @grandfather");
  print_whole_tree(f);

  pr_tag("Inserting father mapping\n");
  UTEST_TRUE(Utest::Expect,
             m.insert (f, &*father, to_pfn(0), to_pfn(0), to_pcnt(O_page)),
             "Insert @father");
  print_whole_tree(f);
  f.clear();

  // GF -> F

  pr_tag("Lookup father at phys=0\n");
  UTEST_TRUE(Utest::Expect, m.lookup(&*father, to_pfn(0), to_pfn(0), &f),
             "Lookup @father");
  print_whole_tree(f);

  pr_tag("Insert son mapping\n");
  UTEST_TRUE(Utest::Expect,
             m.insert (f, &*son, to_pfn(0), to_pfn(0), to_pcnt(O_page)),
             "Insert @son");
  print_whole_tree(f);
  f.clear();

  // GF -> F -> S

  pr_tag("Lost aunt returns from holidays\n");
  UTEST_TRUE(Utest::Expect, m.lookup(&*grandfather, to_pfn(0), to_pfn(0), &f),
             "Lookup @grandfather");
  print_whole_tree(f);

  pr_tag("Insert aunt mapping\n");
  UTEST_TRUE(Utest::Expect,
             m.insert(f, &*aunt, to_pfn(0), to_pfn(0), to_pcnt(O_page)),
             "Insert @aunt");
  print_whole_tree(f);
  f.clear();

  // GF -> F -> S
  //    -> A

  pr_tag("Lookup father at phys=0\n");
  UTEST_TRUE(Utest::Expect, m.lookup(&*father, to_pfn(0), to_pfn(0), &f),
             "Lookup @father");

  print_whole_tree(f);

  pr_tag("Father is killed by his new love\n");
  m.flush(f, L4_map_mask::full(), to_pfn(0), to_pfn(S_page));
  print_whole_tree(f);
  f.clear();

  // GF -> A

  UTEST_FALSE(Utest::Expect, m.lookup(&*father, to_pfn(0), to_pfn(0), &f),
              "Lookup @father");

  UTEST_FALSE(Utest::Expect, m.lookup(&*son, to_pfn(0), to_pfn(0), &f),
              "Lookup @son");
}

PUBLIC
template <typename M>
void
Mapdb_ext_test::test_mapdb_multilevel()
{
  Utest_fw::tap_log.new_test(Mapdb_group, __func__,
                             "f063d80d-7987-423a-8f8b-21bc09aff86d");

  Mapdb &m = create_mapdb(M());
  Mapdb::Frame f, s0_f;
  Mapping *sub;

  enum : Address
  {
    Sigma0_addr_1  =          0,               // size = 1G
    Sigma0_addr_2  = 0x03200000,
    Sigma0_addr_3  = 0x51000000,
    Sigma0_addr_4  = 0xd2000000,               // size = 1 superpage

    Father_addr    = 0x03000000,               // size = 8M

    Aunt_addr_1    =        _2M,
    Aunt_addr_2    =  0x4000000,               // size = 1 superpage
    Aunt_addr_3    =  0x5000000,               // size = 1 superpage

    Son_addr_1     = 0x20000000,               // size = 4M
    Son_addr_2     = 0xa0000000,               // size = 1 page

    Daughter_addr  =        _1G,              // size = 1GB

    Client_addr_1  =              2 * S_page,
    Client_addr_2  =    0x40000 + 2 * S_page,
    Client_addr_3  =    0x80000 + 2 * S_page,
    Client_addr_4  =    0xc0000 + 2 * S_page,
    Client_addr_5  =   0x100000,
    Client_addr_6  = 0x20000000,
  };

  Mapdb::Pfn const poffs = to_pfn(Sigma0_addr_4);

  // In the following we ignore mappings from Sigma0: Sigma0 has 4 x 1GB mapped.

  pr_tag("# Lookup Sigma0_addr_4\n");
  UTEST_TRUE(Utest::Expect, lookup(m, &*sigma0, poffs, &f),
             "Lookup @sigma0 Sigma0_addr_4");
  s0_f = f;
  print_node(f);

  pr_tag("# Insert sub-mapping page @other\n");
  UTEST_TRUE(Utest::Assert, (sub = insert(m, s0_f, &*other, to_pfn(2*S_page),
                                          poffs + to_pcnt(O_page),
                                          to_po(O_page))),
           "Insert sub-mapping page @other");
  print_node(s0_f, sub);
  //s0_f.clear(true);

  // other:
  // 1p @ 2p

  pr_tag("# Get that mapping again\n");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*other, to_pfn(2*S_page), &f),
             "Lookup @other at 2*page");
  print_node(f);
  UTEST_EQ(Utest::Expect, f.treemap->page_shift(), Mapdb::Order(0),
           "Expected order");

  pr_tag("# Insert sub-mapping 2M @other\n");
  UTEST_TRUE(Utest::Expect, (sub = insert(m, s0_f, &*other, to_pfn(_4M),
                                          poffs + to_pcnt(Order(O_2M)),
                                          to_po(O_2M))),
             "Insert sub-mapping 2M @other");
  print_node(s0_f, sub);
  //s0_f.clear(true);

  // other:
  // 1p @ 2p
  // 2M @ 4M

  pr_tag("# Get that mapping again\n");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*other, to_pfn(_4M), &f),
             "Lookup @other at 4M");
  print_node(f);
  UTEST_EQ(Utest::Expect,
           f.treemap->page_shift(), Mapdb::Order(O_2M - O_page),
           "Expected order");

  pr_tag("# Insert sub-mapping 2M @aunt\n");
  UTEST_TRUE(Utest::Expect, (sub = insert(m, s0_f, &*aunt, to_pfn(Aunt_addr_1),
                                          poffs,
                                          to_po(O_2M))),
           "Insert sub-mapping 2M @aunt");
  print_node(s0_f, sub);
  //s0_f.clear();

  // other:
  // 1p @ 2p
  // 2M @ 4M
  //    |
  //    +-------> aunt:
  //              2M @ Aunt_addr_1

  pr_tag("# Map page FROM 4M @other TO Son_addr_2 @son\n");
  UTEST_TRUE(Utest::Expect, map(m, &*other, to_pfn(_4M),
                                &*son, to_pfn(Son_addr_2),
                                to_po(O_page)),
             "Map page FROM 2*2M at other TO Son_addr_2 @son");

  // other:
  // 1p @ 2p
  // 2M @ 4M
  //    |
  //    +-------> aunt:
  //    |         2M @ Aunt_addr_1
  //    |
  //    +-------> son:
  //              2M @ Son_addr_2

  pr_tag("# Unmap 2M FROM 4M @other...\n");
  UTEST_TRUE(Utest::Expect, unmap(m, &*other,
                                  to_pfn(_4M),
                                  to_pfn(_4M + _2M), false),
             "Unmap 2M from 4M @other");

  // other:
  // 1p @ 2p
  // 2M @ 4M

  pr_tag("# Unmap 2M FROM poffs + 2M @sigma0...\n");
  UTEST_TRUE(Utest::Expect, unmap(m, &*sigma0,
                                  poffs + to_pcnt(O_2M),
                                  poffs + to_pcnt(O_2M) + to_pcnt(O_2M), false),
             "Unmap 2M from poffs + 2M @sigma0");

  // other:
  // 1p @ 2p

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*other, to_pfn(2*S_page), &f),
             "Lookup @other at 2*pages");

  pr_tag("# Unmap 1 page FROM 2*page @other\n");
  UTEST_TRUE(Utest::Expect, unmap(m, &*other,
                                  to_pfn(2 * S_page),
                                  to_pfn(2 * S_page + S_page), true),
             "Unmap from other");

  // no mappings (except Sigma0)

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*other, to_pfn(2*S_page), &f),
              "Lookup @other at 2*pages");

  pr_tag("# Map 2*4MB FROM Sigma0_addr_3 @sigma0 TO Father_addr @father\n");
  UTEST_TRUE(Utest::Expect, map(m, &*sigma0, to_pfn(Sigma0_addr_3),
                                &*father, to_pfn(Father_addr),
                                to_po(O_4M)),
             "Map 4MB from Sigma0_addr_3 @sigma0 to Father_addr @father");
  UTEST_TRUE(Utest::Expect, map(m, &*sigma0, to_pfn(Sigma0_addr_3 + _4M),
                                &*father, to_pfn(Father_addr + _4M),
                                to_po(O_4M)),
             "Map 4MB from Sigma0_addr_3 + 4M @sigma0 to Father_addr + 4M @father");

  // father:
  // 8M @ Father_addr

  sub = *f.m;
  pr_tag("# Get first 8MB mapping\n");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*father, to_pfn(Father_addr), &f),
             "Lookup @father at Father_addr");
  print_node(f, sub);
  f.clear();

  pr_tag("# Map 3*2MB FROM Father_addr + 2M @father TO Aunt_addr_2 @aunt\n");
  for (unsigned i = 0; i < 3; ++i)
    UTEST_TRUE(Utest::Expect, map(m, &*father,
                                    to_pfn(Father_addr + _2M + (i<<(O_2M))),
                                  &*aunt, to_pfn(Aunt_addr_2 + (i<<(O_2M))),
                                  to_po(O_2M)),
               "Map 6M from Father_addr + 2M @father to Aunt_addr_2 @aunt");

  // father:
  // 8M @ Father_addr
  //    |
  //    +- +2M -> aunt:
  //              2M @ Aunt_addr_2
  //              2M @ Aunt_addr_2+2M
  //              2M @ Aunt_addr_2+4M

  pr_tag("# Map 3 pages FROM Aunt_addr_2 + page @aunt TO Client_addr_1 @client\n");
  for (unsigned i = 0; i < 3; ++i)
    UTEST_TRUE(Utest::Expect, map(m, &*aunt,
                                    to_pfn(Aunt_addr_2 + S_page + (i<<O_page)),
                                  &*client, to_pfn(Client_addr_1 + (i<<O_page)),
                                  to_po(O_page)),
               "Map 3 pages from Aunt_addr_2 + page @aunt to Client_addr_1 @client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +- +2M -> aunt:
  //              2M @ Aunt_addr_2
  //                 |
  //                 +- +1p ---------------------------> client:
  //                                                     3p @ 2p
  //              2M @ Aunt_addr_2+2M
  //              2M @ Aunt_addr_2+2M

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_1 - S_page), &f),
              "Lookup @client at Client_addr_1 - page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_1), &f),
             "Lookup @client at Client_addr_1");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_1 + S_page), &f),
             "Lookup @client at Client_addr_1 + page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_1 + 2*S_page), &f),
             "Lookup @client at Client_addr_1 + 2*page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_1 + 3*S_page), &f),
              "Lookup @client at Client_addr_1 + 3*page");

  pr_tag("# Unmap 1 page FROM Father_addr + 2M + 2*page @father\n");
  UTEST_TRUE(Utest::Expect, unmap(m, &*father,
                                  to_pfn(Father_addr + _2M + S_page*2),
                                  to_pfn(Father_addr + _2M + S_page*3),
                                  false),
             "Unmap Father_addr + 2M + 2*page");

  // father:
  // 8M @ Father_addr
  //
  // (All mappings from aunt and client were removed because the mappings to
  // aunt had a granularity of 2M.)

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*father, to_pfn(Father_addr + _2M + S_page*2), &f),
             "Lookup @father at Father_addr + 2M + 2*page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*aunt, to_pfn(Aunt_addr_2), &f),
              "Lookup @aunt at Aunt_addr_2");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_1 - S_page), &f),
              "Lookup @client at Client_addr_1 - page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_1), &f),
              "Lookup @client at Client_addr_1");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_1 + S_page), &f),
              "Lookup @client at Client_addr_1 + page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_1 + 2*S_page), &f),
              "Lookup @client at Client_addr_1 + 2*page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_1 + 3*S_page), &f),
              "Lookup @client at Client_addr_1 + 3*page");

  pr_tag("# Map 4M FROM Father_addr @father TO Aunt_addr_3 @aunt\n");
  UTEST_TRUE(Utest::Expect, map(m, &*father, to_pfn(Father_addr),
                                &*aunt, to_pfn(Aunt_addr_3),
                                to_po(O_4M)),
             "Map 4M from Father_addr @father to Aunt_addr_3 @aunt");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3

  pr_tag("# Map 3 pages FROM aunt at Aunt_addr_3 + 2M + page TO client at Client_addr_2\n");
  for (unsigned i = 0; i < 3; ++i)
    UTEST_TRUE(Utest::Expect, map(m, &*aunt, to_pfn(Aunt_addr_3 + _2M
                                                    + S_page + (i<<O_page)),
                                  &*client, to_pfn(Client_addr_2 + (i<<O_page)),
                                  to_po(O_page)),
               "Map page from Aunt_addr_3 + 2M + page + x @aunt to Client_addr_2 @client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+1p
  //                                                     1p @ Client_addr_2+2p

  pr_tag("# Unmap 1 page FROM Aunt_addr_3 + 2M + page + page @aunt\n");
  UTEST_TRUE(Utest::Expect, unmap(m, &*aunt,
                                  to_pfn(Aunt_addr_3 + _2M + S_page + S_page*1),
                                  to_pfn(Aunt_addr_3 + _2M + S_page + S_page*2),
                                  false),
             "Unmap Aunt_addr_3 + 2M + page + page from aunt");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*father, to_pfn(Father_addr + _2M + S_page*2), &f),
             "Lookup @father at Father_addr + 2M + 2*page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*aunt, to_pfn(Aunt_addr_3), &f),
             "Lookup @aunt at Aunt_addr_3 + 2M");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_2 - S_page), &f),
              "Lookup @client at Client_addr_2 - page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_2), &f),
             "Lookup @client at Client_addr_2");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_2 + S_page), &f),
              "Lookup @client at Client_addr_2 + page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_2 + 2*S_page), &f),
             "Lookup @client at Client_addr_2 + 2*page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_2 + 3*S_page), &f),
              "Lookup @client at Client_addr_2 + 3*page");

  pr_tag("# Map 3 pages FROM Sigma0_addr_2 - page @sigma0 TO Client_addr_3 @client\n");
  for (unsigned i = 0; i < 3; ++i)
    UTEST_TRUE(Utest::Expect, map(m, &*sigma0, to_pfn(Sigma0_addr_2 - S_page
                                                      + (i<<O_page)),
                                  &*client, to_pfn(Client_addr_3 + (i<<O_page)),
                                  to_po(O_page)),
               "Map page at Sigma0_addr_2 - page sigma0 to Client_addr_3 @client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_3 - S_page), &f),
              "Lookup @client at Client_addr_3 - page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_3), &f),
             "Lookup @client at Client_addr_3");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_3 + S_page), &f),
             "Lookup @client at Client_addr_3 + page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_3 + 2*S_page), &f),
             "Lookup @client at Client_addr_3 + 2*page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_3 + 3*S_page), &f),
              "Lookup @client at Client_addr_3 + 3*page");

  pr_tag("# Map 1G from Sigma0_addr_1 @sigma0 to Daughter_addr @daughter\n");
  UTEST_TRUE(Utest::Expect, map(m, &*sigma0, to_pfn(Sigma0_addr_1),
                                &*daughter, to_pfn(Daughter_addr),
                                to_po(O_1G)),
             "Map sigma0 to daughter");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*daughter, to_pfn(Daughter_addr - 1), &f),
              "Lookup @daughter at Daughter_addr - 1");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*daughter, to_pfn(Daughter_addr + _256M), &f),
             "Lookup @daughter at Daughter_addr + 256M");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*daughter, to_pfn(Daughter_addr + _1G), &f),
              "Lookup @daughter at Daughter_addr + 1G");

  pr_tag("# Map 3 pages FROM daughter TO client at Client_addr_4\n");
  for (unsigned i = 0; i < 3; ++i)
    UTEST_TRUE(Utest::Expect, map(m, &*daughter, to_pfn(Daughter_addr
                                                        + 3 * _16M
                                                        + _2M
                                                        - S_page
                                                        + (i<<O_page)),
                                  &*client, to_pfn(Client_addr_4 + (i<<O_page)),
                                  to_po(O_page)),
               "Map daughter to client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr
  //                           |
  //                           +- +50M - page ---------> client:
  //                                                     1p @ Client_addr_4
  //                                                     1p @ Client_addr_4+1p
  //                                                     1p @ Client_addr_4+2p

  pr_tag("# Map 2*2M FROM Daughter_addr + 3*16M @daughter to Client_addr_6 @client\n");
  UTEST_TRUE(Utest::Expect, map(m, &*daughter, to_pfn(Daughter_addr + 3*_16M),
                                &*client, to_pfn(Client_addr_6),
                                to_po(Order(O_2M))),
             "Map 2M from Daughter_addr + 3*16M @daughter to Client_addr_6 @client");
  UTEST_TRUE(Utest::Expect, map(m, &*daughter, to_pfn(Daughter_addr + 3*_16M + _2M),
                                &*client, to_pfn(Client_addr_6 + _2M),
                                to_po(Order(O_2M))),
             "Map 2M from Daughter_addr + 3*16M + 2M @daughter to Client_addr_6 + 2M @client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr
  //                           |
  //                           +- +50M - page ---------> client:
  //                           |                         1p @ Client_addr_4
  //                           |                         1p @ Client_addr_4+1p
  //                           |                         1p @ Client_addr_4+2p
  //                           |
  //                           +- +50M ----------------> client:
  //                                                     2M @ Client_addr_6
  //                                                     2M @ Client_addr_6+2M

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_4 - S_page), &f),
              "Lookup @client at Client_addr_4 - page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_4), &f),
             "Lookup @client at Client_addr_4");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_4 + S_page), &f),
             "Lookup @client at Client_addr_4 + page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_4 + 2*S_page), &f),
             "Lookup @client at Client_addr_4 + 2*page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_4 + 3*S_page), &f),
              "Lookup @client at Client_addr_4 + 3*page");

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_6), &f),
             "Lookup @client at Client_addr_6");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_6 + _2M), &f),
             "Lookup @client at Client_addr_6 + 2M");

  pr_tag("# Map 4M from Daughter_addr + 3*16M @daughter to Son_addr_1 @son\n");
  UTEST_TRUE(Utest::Expect, map(m, &*daughter, to_pfn(Daughter_addr + 3*_16M),
                                &*son, to_pfn(Son_addr_1),
                                to_po(O_4M)),
             "Map daughter to son");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr
  //                           |
  //                           +- +50M - page ---------> client:
  //                           |                         1p @ Client_addr_4
  //                           |                         1p @ Client_addr_4+1p
  //                           |                         1p @ Client_addr_4+2p
  //                           |
  //                           +- +50M ----------------> client:
  //                           |                         2M @ Client_addr_6
  //                           |                         2M @ Client_addr_6+2M
  //                           |
  //                           +- +48M -> son:
  //                                      4M @ Son_addr_1

  pr_tag("# Map page from Son_addr_1 + 2M @son to Client_addr_5 @client\n");
  UTEST_TRUE(Utest::Expect, map(m, &*son, to_pfn(Son_addr_1 + _2M),
                                &*client, to_pfn(Client_addr_5),
                                to_po(O_page)),
             "Map son to client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr
  //                           |
  //                           +- +50M - page ---------> client:
  //                           |                         1p @ Client_addr_4
  //                           |                         1p @ Client_addr_4+1p
  //                           |                         1p @ Client_addr_4+2p
  //                           |
  //                           +- +50M ----------------> client:
  //                           |                         2M @ Client_addr_6
  //                           |                         2M @ Client_addr_6+2M
  //                           |
  //                           +- +48M -> son:
  //                                      4M @ Son_addr_1
  //                                         |
  //                                         +- +2M ---> client:
  //                                                     1p @ Client_addr_5

  pr_tag("# Map page from Son_addr_1 + 2M + page @son to Client_addr_5 + page @client\n");
  UTEST_TRUE(Utest::Expect, map(m, &*son, to_pfn(Son_addr_1 + _2M + S_page),
                                &*client, to_pfn(Client_addr_5 + S_page),
                                to_po(O_page)),
             "Map son to client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr
  //                           |
  //                           +- +50M - page ---------> client:
  //                           |                         1p @ Client_addr_4
  //                           |                         1p @ Client_addr_4+1p
  //                           |                         1p @ Client_addr_4+2p
  //                           |
  //                           +- +50M ----------------> client:
  //                           |                         2M @ Client_addr_6
  //                           |                         2M @ Client_addr_6+2M
  //                           |
  //                           +- +48M -> son:
  //                                      4M @ Son_addr_1
  //                                         |
  //                                         +- +2M ---> client:
  //                                         |           1p @ Client_addr_5
  //                                         |
  //                                         +- +2M+1p > client:
  //                                                     1p @ Client_addr+1p

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_5), &f),
             "Lookup @client at Client_addr_5");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_5 + S_page), &f),
             "Lookup @client at Client_addr_5 + page");

  show_tree(m.dbg_treemap());

  pr_tag("# Unmap 1 page FROM Daughter_addr + 3*16M + 2M @daughter\n");
  UTEST_TRUE(Utest::Expect, unmap(m, &*daughter,
                                  to_pfn(Daughter_addr + 3*_16M + _2M),
                                  to_pfn(Daughter_addr + 3*_16M + _2M + S_page), false),
             "Unmap Daughter_addr + 3*16M + 2M");
  show_tree(m.dbg_treemap());

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr
  //                           |
  //                           +- +50M - page ---------> client:
  //                           |                         1p @ Client_addr_4
  //                           |                         1p @ Client_addr_4+2p
  //                           |
  //                           +- +50M ----------------> client:
  //                                                     2M @ Client_addr_6
  //                                                     2M @ Client_addr_6+2M

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_5), &f),
              "Lookup @client at Client_addr_5");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_5 + S_page), &f),
              "Lookup @client at Client_addr_5 + page");

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_4), &f),
             "Lookup @client at Client_addr_4");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_4 + S_page), &f),
              "Lookup @client at Client_addr_4 + page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_4 + 2*S_page), &f),
             "Lookup @client at Client_addr_4 + 2*page");

  pr_tag("# Map page from Daughter_addr + 3*16M + 2M @daughter to Client_addr_4 + page @client\n");
  UTEST_TRUE(Utest::Expect, map(m, &*daughter, to_pfn(Daughter_addr + 3*_16M + _2M),
                                &*client, to_pfn(Client_addr_4 + S_page),
                                to_po(O_page)),
             "Map daughter to client");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+1p
  //                                                     1p @ Client_addr_3+2p
  // 1G @ Sigma0_addr_1
  //    |
  //    +-------> daughter: 1G @ Daughter_addr
  //                           |
  //                           +- +50M - page ---------> client:
  //                           |                         1p @ Client_addr_4
  //                           |                         1p @ Client_addr_4+2p
  //                           |
  //                           +- +50M ----------------> client:
  //                           |                         1p @ Client_addr_4+1p
  //                           |
  //                           +- +50M ----------------> client:
  //                                                     2M @ Client_addr_6
  //                                                     2M @ Client_addr_6+2M

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_4 + S_page), &f),
             "Lookup @client at Client_addr_4 + page");

  pr_tag("# Unmap 1 page FROM Sigma0_addr_2 @sigma0\n");
  show_tree(m.dbg_treemap());
  UTEST_TRUE(Utest::Expect, unmap(m, &*sigma0,
                                  to_pfn(Sigma0_addr_2),
                                  to_pfn(Sigma0_addr_2 + S_page), false),
             "Unmap Sigma0_addr_2");

  // father:
  // 8M @ Father_addr
  //    |
  //    +------> aunt:
  //             4M @ Aunt_addr_3
  //                |
  //                +- +2M+page -----------------------> client:
  //                                                     1p @ Client_addr_2
  //                                                     1p @ Client_addr_2+2p
  // sigma0:
  // 3p @ Sigma0_addr_2 - page
  //    |
  //    +----------------------------------------------> client:
  //                                                     1p @ Client_addr_3
  //                                                     1p @ Client_addr_3+2p

  // (Sigma0_addr_2 is included in the 1G @ Sigma0_addr_1 mapping, hence the
  // Daughter_addr mapping was also removed.)

  UTEST_TRUE(Utest::Expect,
             lookup(m, &*sigma0, to_pfn(Sigma0_addr_2), &f),
             "Lookup @sigma0 at Sigma0_addr_2");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_3), &f),
             "Lookup @client at Client_addr_3");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_3 + S_page), &f),
              "Lookup @client at Client_addr_3 + page");
  UTEST_TRUE(Utest::Expect,
             lookup(m, &*client, to_pfn(Client_addr_3 + 2*S_page), &f),
             "Lookup @client at Client_addr_3 + 2*page");

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_4), &f),
              "Lookup @client at Client_addr_4");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_4 + S_page), &f),
              "Lookup @client at Client_addr_4 + page");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_4 + 2*S_page), &f),
              "Lookup @client at Client_addr_4 + 2*page");

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_6), &f),
              "Lookup @client at Client_addr_6");
  UTEST_FALSE(Utest::Expect,
              lookup(m, &*client, to_pfn(Client_addr_6 + _2M), &f),
              "Lookup @client at Client_addr_6 + 2M");

  UTEST_FALSE(Utest::Expect,
              lookup(m, &*daughter, to_pfn(Daughter_addr), &f),
              "Lookup @daughter at Daughter_addr");
  show_tree(m.dbg_treemap());
}
