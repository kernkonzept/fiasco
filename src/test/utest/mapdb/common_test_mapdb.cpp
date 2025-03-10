/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 */

INTERFACE:

#include "config.h"
#include "mapdb.h"
#include "mem_space.h"
#include "types.h"
#include "utest_fw.h"

// Intermediate solution for Utest_fw::prtag() or something similar which will
// - perform a single printf() call for printing the tag and the format +
//   parameters
// - prefix every line with the tag in multi-line formats
#define pr_tag(format, ...) printf("@@ mdb:" format, ##__VA_ARGS__)

class Mapdb_test_base
{
public:
  static constexpr Address _16K    =    16 << 10;
  static constexpr Address _64K    =    64 << 10;
  static constexpr Address _512K   =   512 << 10;
  static constexpr Address _1M     =     1 << 20;
  static constexpr Address _2M     =   _1M <<  1;
  static constexpr Address _4M     =   _2M <<  1;
  static constexpr Address _8M     =   _4M <<  1;
  static constexpr Address _16M    =   _8M <<  1;
  static constexpr Address _32M    =  _16M <<  1;
  static constexpr Address _64M    =  _32M <<  1;
  static constexpr Address _128M   =  _64M <<  1;
  static constexpr Address _256M   = _128M <<  1;
  static constexpr Address _1G     =     1 << 30;
  static constexpr Address _2G     =   _1G <<  1;
  static constexpr Address S_page  = Config::PAGE_SIZE;
  static constexpr Address S_super = Config::SUPERPAGE_SIZE;
  static constexpr Address Pages_per_super = Address{Config::SUPERPAGE_SIZE}
                                           / Address{Config::PAGE_SIZE};

  enum Order
  {
    O_page = Config::PAGE_SHIFT,
    O_super = Config::SUPERPAGE_SHIFT,
    O_1M = 20,
    O_2M = 21,
    O_4M = 22,
    O_1G = 30,
  };

  Mapdb_test_base()
  {
    pr_tag("\n");
    pr_tag("=== NEW TEST ===\n");
  }

  ~Mapdb_test_base()
  {
    pr_tag("=== DONE TEST ===\n");
    pr_tag("\n");
    Utest_fw::tap_log.test_done();
  }
};

class Test_space : public Space
{
public:
  Test_space(Ram_quota *rq, char const *name)
  : Space(rq, Caps::all()), name(name)
  {
    inc_ref(); // Ensure space is not deleted by reference counting.
    Utest_fw::chk(initialize(), "Initialize space object");
  }

  char const *const name;
};

class Test_s0_space : public Test_space
{
public:
  explicit Test_s0_space(Ram_quota *q) : Test_space(q, "sigma0") {}
  bool is_sigma0() const override { return true; }
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC
bool
Test_s0_space::v_fabricate(Mem_space::Vaddr address,
                           Mem_space::Phys_addr *phys,
                           Mem_space::Page_order *order,
                           Mem_space::Attr *attr = nullptr) override
{
  // Special-cased because we don't do page table lookup for sigma0.
  *order = static_cast<Mem_space const &>(*this).largest_page_size();
  if (*order > Mem_space::Page_order(Config::SUPERPAGE_SHIFT))
    *order = Mem_space::Page_order(Config::SUPERPAGE_SHIFT);
  *phys = cxx::mask_lsb(Virt_addr(address), *order);
  if (attr)
    *attr = Mem_space::Attr::space_local(L4_fpage::Rights::URWX());

  return true;
}

/**
 * Convert a virtual address into a page frame number.
 */
PROTECTED static inline
Mapdb::Pfn
Mapdb_test_base::to_pfn(Address a)
{ return Mem_space::to_pfn(Virt_addr(a)); }

/**
 * Convert a page frame number into a human-readable virtual address.
 */
PROTECTED static inline
Address
Mapdb_test_base::to_virt(Mapdb::Pfn pfn)
{ return cxx::int_value<Mapdb::Pfn>(pfn) << Virt_addr::Shift; }

PROTECTED static
char const *
Mapdb_test_base::node_name(Space *s)
{
  auto const *ts = static_cast<Test_space const *>(s);
  return ts ? ts->name : "<NULL>";
}

//---------------------------------------------------------------------------
IMPLEMENTATION[mips]:

#include "config.h"

PROTECTED static inline NEEDS["config.h"]
bool
Mapdb_test_base::have_superpages()
{ return Config::have_superpages; }

//---------------------------------------------------------------------------
IMPLEMENTATION[!mips]:

PROTECTED static inline
bool
Mapdb_test_base::have_superpages()
{ return Cpu::have_superpages(); }
