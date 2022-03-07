/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 */

/**
 * fpage_unmap() / Mem_space::vdelete():
 *   These functions are supposed to return the access status of unmapped pages
 *   but the implementation depends on the architecture:
 *   - x86/AMD64: return `RW` if the page was 'dirty', otherwise return `R` if
 *     the page was 'accessed', otherwise return 0
 *   - ARM/ARM64: always return 0
 *   - MIPS: return the combined access rights of the physical page from the
 *     page table (how the page could be accessed before the unmap operation)
 *
 * v_set_access_flags():
 *   This function is supposed to set the access flags of mapped pages and the
 *   implementation depends on the architecture:
 *    - x86/AMD64: the access flags of the mapped pages are modified
 *    - ARM/ARM64: the mapping is not modified
 *    - MIPS: the mapping is not modified
 */

INTERFACE:

static char const __attribute__((unused)) *Mapdb_group = "Mapdb";

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "utest_fw.h"
#include "kmem_alloc.h"
#include "mapdb.h"
#include "map_util.h"
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

  Mapdb_util_test().test_map_util();

  Utest_fw::tap_log.finish();
}

class Mapdb_util_test : public Mapdb_test_base
{
public:
  Mapdb_util_test()
  : sigma0(&rq)
  {
    // mapdb_mem only exported from map_util-mem for debug builds
    extern Static_object<Mapdb> mapdb_mem;
    init_mapdb_mem(&sigma0);
    mapdb = mapdb_mem.get();
  }

private:
  Mapdb *mapdb;
  Test_fake_factory rq;
  Test_s0_space sigma0;
};

/**
 * L4_map_mask: Unmap from others, not from my address space.
 */
PRIVATE static inline
L4_map_mask
Mapdb_util_test::not_me() { return L4_map_mask(0); }

/**
 * L4_map_mask: Unmap from all address spaces, also from mine.
 */
PRIVATE static inline
L4_map_mask
Mapdb_util_test::also_me() { return L4_map_mask(0x80000000); }

/**
 * Convert a `Space` into a `Mem_space`.
 */
PRIVATE static inline
Mem_space *
Mapdb_util_test::ms(Space *s)
{ return s; }

/**
 * Convert a virtual address into a Mem_space::Vaddr as required by the
 * parameter of Mem_space::v_lookup().
 */
PRIVATE static inline
Mem_space::Vaddr
Mapdb_util_test::to_vaddr(Address a)
{ return Mem_space::Vaddr(Virt_addr(a)); }

/**
 * Create an L4_msg_item from the map base.
 */
PRIVATE static
L4_msg_item
Mapdb_util_test::map_base(Address base)
{ return L4_msg_item::map(base); }

/**
 * Print information about the node.
 *
 * The output is compared against stored output.
 */
PRIVATE
void
Mapdb_util_test::print_node(Space *space, Mapdb::Pfn pfn,
                            Address va_begin = 0UL, Address va_end = ~0UL)
{
  pr_tag("MapDB node at 0x%lx\n", to_virt(pfn));
  Mapdb::Frame frame;
  if (!mapdb->lookup(space, pfn, pfn, &frame))
    {
      pr_tag("no mappings\n");
      return;
    }

  auto const node = *frame.m ? frame.m : frame.frame->first();
  UTEST_NE(Utest::Assert, nullptr, *node, "Page frame has node");

  printf(" space=%s vaddr=0x%lx size=0x%lx\n",
         node_name(frame.pspace()), to_virt(frame.pvaddr()),
         to_virt(Mapdb::Pfn(1) << frame.treemap->page_shift()));

  Mapdb::foreach_mapping(frame, to_pfn(va_begin), to_pfn(va_end),
                         [](Mapping *node, Mapdb::Order order)
    {
      pr_tag("%*sspace=%s vaddr=0x%lx size=0x%lx\n",
             (int)node->depth() + 1, "", node_name(node->space()),
             to_virt(node->pfn(order)), to_virt(Mapdb::Pfn(1) << order));
    });
  pr_tag("\n");
}

/**
 * Create spaces, create mappings between the spaces and verify the state of
 * the mapping DB after each state.
 *
 * This test is quite large because splitting it into several small tests would
 * introduce a lot of code duplication. Here we rather create and delete
 * mappings step by step and print the relevant state of the mapping DB after
 * each step. The tapper-wrapper will compare the test output with expected
 * output.
 */
PUBLIC
void
Mapdb_util_test::test_map_util()
{
  Utest_fw::tap_log.new_test(Mapdb_group, __func__,
                             "d32fa52b-7faa-4a17-abb3-3c5ceeac3e0f");

  unsigned const order_max = have_superpages() ? O_super : O_page;

  static_assert(S_super >= _1M, "Adapt test for smaller superpage size");

  pr_tag("Page = %ldKB, Superpage = %ldMB\n",
         (Address)S_page >> 10, (Address)S_super >> 20);

  // Support for pages > superpage is optional.
  printf(" largest_page_size = %d\n",
         cxx::int_value<Mem_space::Page_order>(sigma0.largest_page_size()));

  pr_tag("have_superpages = %s\n", have_superpages() ? "yes" : "no");
  pr_tag("\n");

  auto server = Utest::kmem_create<Test_space>(&rq, "server");
  auto client = Utest::kmem_create<Test_space>(&rq, "client");

  typedef Mem_space::Page_order Page_order;
  typedef Mem_space::Phys_addr Phys_addr;
  typedef Mem_space::Attr Attr;
  typedef L4_fpage::Rights Rights;

  Phys_addr phys;
  Page_order order;
  Attr attr;
  Kobject::Reap_list rl;

  // 1: MAP sigma0[64K/page] -> server[ALL:16K]
  pr_tag("MAP sigma0[64K/page] -> server[ALL:page]\n");
  pr_tag("=> single page mapped to server\n");
  UTEST_FALSE(Utest::Assert,
              ms(&*server)->v_lookup(to_vaddr(_16K), &phys, &order, &attr),
              "VA server:16K nothing mapped");
  UTEST_NOERR(Utest::Assert,
              fpage_map(&sigma0, L4_fpage::mem(_64K, O_page, Rights::URWX()),
                        &*server, L4_fpage::all_spaces(), map_base(_16K), &rl),
              "Map sigma0[64K/page] to server[16K]");
  UTEST_TRUE(Utest::Assert,
             ms(&*server)->v_lookup(to_vaddr(_16K), &phys, &order, &attr),
             "VA 16K successful lookup");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA 16K mapped with expected page order");
  UTEST_EQ(Utest::Expect, Phys_addr(_64K), phys,
           "VA 16K mapped to expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA 16K mapped with expected rights");
  print_node(&sigma0, to_pfn(_64K));

  // 2: MAP sigma0[0/superpage] -> server[ALL:0]
  //    Should map many pages and overmap previous mapping
  pr_tag("MAP sigma0[0/superpage] -> server[ALL:0]\n");
  pr_tag("=> many pages mapped to server overmapping previous mapping\n");
  UTEST_FALSE(Utest::Assert,
              ms(&*server)->v_lookup(to_vaddr(0), &phys, &order, &attr),
              "VA server:0 nothing mapped");
  UTEST_NOERR(Utest::Assert,
              fpage_map(&sigma0, L4_fpage::mem(0, O_super, Rights::URX()),
                        &*server, L4_fpage::all_spaces(), map_base(0), &rl),
              "Map sigma0[0/superpage] to server[0]");
  UTEST_TRUE(Utest::Assert,
             ms(&*server)->v_lookup(to_vaddr(0), &phys, &order, &attr),
             "VA 0 mapped");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA 0 mapped with expected page order (not superpage!)");
  UTEST_EQ(Utest::Expect, Phys_addr(0), phys,
           "VA 0 mapped to expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URX(), attr.rights,
           "VA 0 mapped with expected rights");
  print_node(&sigma0, to_pfn(_64K), 0UL, S_super);

  // 3: Verify that the MapDB entry for 64K has changed.
  pr_tag("Verify that the MapDB entry for 64K has changed\n");
  UTEST_TRUE(Utest::Assert,
             ms(&*server)->v_lookup(to_vaddr(_16K), &phys, &order, &attr),
             "VA server:16K mapped");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA server:16K mapped with expected page order");
  UTEST_EQ(Utest::Expect, Phys_addr(_16K), phys,
           "VA server:16K mapped to changed physical address");
  UTEST_EQ(Utest::Expect, Rights::URX(), attr.rights,
           "VA server:16K mapped with changed rights");
  print_node(&sigma0, to_pfn(_64K), _64K, _64K + _16K);

  // 4: Partially unmap superpage sigma0[0/superpage]
  pr_tag("UNMAP sigma0[512K/%ldK]\n", 1UL << (O_super - 3 - 10));
  pr_tag("=> remove couple of page mappings from server\n");
  UTEST_TRUE(Utest::Assert,
             ms(&*server)->v_lookup(to_vaddr(_512K + _16K), &phys, &order, &attr),
             "VA server:1M + 16K mapped");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA server:512K+16K mapped with expected order");
  UTEST_EQ(Utest::Expect, Phys_addr(_512K + _16K), phys,
           "VA server:512K+16K mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URX(), attr.rights,
           "VA server:512K+16K mapped with expected attributes");
  fpage_unmap(&sigma0,
              L4_fpage::mem(_512K, O_super - 3, Rights::URWX()),
              not_me(), rl.list());
  print_node(&sigma0, to_pfn(0));

  // 5: MAP sigma0[superpage/superpage] -> server[2*superpage/superpage:0]
  pr_tag("MAP sigma0[superpage/superpage] -> server[2*superpage/superpage:0]\n");
  pr_tag("=> additional 1 superpage mapped to server\n");
  UTEST_FALSE(Utest::Assert,
              ms(&*server)->v_lookup(to_vaddr(2 * S_super), &phys, &order, &attr),
              "VA server:2*superpage nothing mapped");
  UTEST_NOERR(Utest::Assert,
              fpage_map(&sigma0, L4_fpage::mem(S_super, O_super, Rights::URWX()),
                        &*server, L4_fpage::mem(2 * S_super, O_super),
                        map_base(0), &rl),
              "Map sigma0[superpage/superpage] to server[2*superpage]");
  UTEST_TRUE(Utest::Assert,
             ms(&*server)->v_lookup(to_vaddr(2 * S_super), &phys, &order, &attr),
             "VA server:2*superpage MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(order_max), order,
           "VA server:2*superpage mapped with expected order");
  UTEST_EQ(Utest::Expect, Virt_addr(S_super), phys,
           "VA server:2*superpage mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA server:2*superpage mapped with expected rights");
  print_node(&sigma0, to_pfn(S_super), S_super, S_super + S_super);

  // 6: MAP server[2*superpage+page/page] -> client[WHOLE:8*page]
  pr_tag("MAP server[2*superpage+page/page] -> client[WHOLE:8*page]\n");
  pr_tag("=> 1 client mapping added\n");
  UTEST_FALSE(Utest::Assert,
              ms(&*client)->v_lookup(to_vaddr(8 * S_page), &phys, &order, &attr),
              "VA client:8*page not nothing mapped");
  UTEST_NOERR(Utest::Assert,
              fpage_map(&*server, L4_fpage::mem(2 * S_super + S_page, O_page,
                                                Rights::URWX()),
                        &*client, L4_fpage::mem(0, L4_fpage::Whole_space),
                        map_base(8 * S_page), &rl),
              "Map server[2*superpage+page/page] to client[8*page]");
  UTEST_TRUE(Utest::Assert,
             ms(&*client)->v_lookup(to_vaddr(8 * S_page), &phys, &order, &attr),
             "VA client:8*page MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA client:8*page mapped with expected order");
  UTEST_EQ(Utest::Expect, Virt_addr(S_super + S_page), phys,
           "VA client:8*page mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA client:8*page mapped with expected rights");
  print_node(&sigma0, to_pfn(S_super), S_super, S_super + S_super);

  // 7: Overmap an RX. The writable attribute should not be flushed.
  UTEST_NOERR(Utest::Assert,
              fpage_map(&*server, L4_fpage::mem(2 * S_super + S_page, O_page,
                                                Rights::URX()),
                        &*client, L4_fpage::mem(0, L4_fpage::Whole_space),
                        map_base(8 * S_page), &rl),
              "Map server[2*superpage+page/page] read-execute to client");
  UTEST_TRUE(Utest::Assert,
             ms(&*client)->v_lookup(to_vaddr(8 * S_page), &phys, &order, &attr),
             "VA client:8*page MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA client:8*page mapped with expected order");
  UTEST_EQ(Utest::Expect, Virt_addr(S_super + S_page), phys,
           "VA client:8*page mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA client:8*page mapped with full rights; write right not removed");

  // 8: Touch client[8*page] (works only on x86/AMD64)
  //    Note that this operation might be a NOP depending on the architecture.
  ms(&*client)->v_set_access_flags(to_vaddr(8 * S_page), Rights::RW());
  UTEST_TRUE(Utest::Assert,
             ms(&*client)->v_lookup(to_vaddr(8 * S_page), &phys, &order, &attr),
             "VA client:8*page MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA client 8*page mapped with expected order");
  UTEST_EQ(Utest::Expect, Virt_addr(S_super + S_page), phys,
           "VA client 8*page mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA client 8*page mapped with expected rights");

  // 9: Reset dirty from server (works only on x86/AMD64)
  fpage_unmap(&*server, L4_fpage::mem(2 * S_super + S_page, O_page),
              not_me(), rl.list());
  UTEST_TRUE(Utest::Assert,
             ms(&*client)->v_lookup(to_vaddr(8 * S_page), &phys, &order, &attr),
             "VA client 8*page MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA client 8*page mapped with expected order");
  UTEST_EQ(Utest::Expect, Virt_addr(S_super + S_page), phys,
           "VA client 8*page mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA client 8*page mapped with expected rights");

  // 10: Delete client[8*page/page]
  ms(&*client)->v_delete(to_vaddr(8 * S_page), Page_order(O_page), Rights(0));
  UTEST_TRUE(Utest::Assert,
             ms(&*server)->v_lookup(to_vaddr(2 * S_super + S_page), &phys,
                                    &order, &attr),
             "VA server:2*superpage+page MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(order_max), order,
           "VA server 2*superpage+page mapped with expected order");
  UTEST_EQ(Utest::Expect,
           have_superpages() ? Virt_addr(S_super)
                             : Virt_addr(S_super + S_page),
           phys,
           "VA server 2*superpage+page mapped with expected physical address");

  // 11: Flush dirty and accessed from server
  fpage_unmap(&*server,
              L4_fpage::mem(2 * S_super, O_super), also_me(), rl.list());
  UTEST_TRUE(Utest::Assert,
             ms(&*client)->v_lookup(to_vaddr(8 * S_page), &phys, &order, &attr),
             "VA client 8*page MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(O_page), order,
           "VA client 8*page mapped with expected order");
  UTEST_EQ(Utest::Expect, Virt_addr(S_super + S_page), phys,
           "VA client 8*page mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA client 8*page mapped with expected rights");

  // 12: Delete client[8*page/page]
  ms(&*client)->v_delete(to_vaddr(8 * S_page), Page_order(O_page), Rights(0));
  UTEST_TRUE(Utest::Assert,
             ms(&*server)->v_lookup(to_vaddr(2 * S_super), &phys, &order, &attr),
             "VA client 8*page MapDB lookup");
  UTEST_EQ(Utest::Expect, Page_order(order_max), order,
           "VA client 8*page mapped with expected order");
  UTEST_EQ(Utest::Expect, Virt_addr(S_super), phys,
           "VA client 8*page mapped with expected physical address");
  UTEST_EQ(Utest::Expect, Rights::URWX(), attr.rights,
           "VA client 8*page mapped with expected rights");
}
