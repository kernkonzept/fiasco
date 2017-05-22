IMPLEMENTATION:

#include <cstdio>
#include <cctype>

#include "alternatives.h"
#include "config.h"
#include "jdb.h"
#include "jdb_module.h"

#include "paging.h"
#include "mem_unit.h"
#include "cpu.h"

namespace {

template<typename I, typename C>
void sort(I b, I e, C c)
{
  if (b == e)
    return;

  I end = e;
  for (;;)
    {
      auto newn = b;
      auto y = b;
      auto x = b;
      ++y;
      for (; y != end; ++x, ++y)
        {
          if (c(*x, *y))
            {
              auto t = *x;
              *x = *y;
              *y = t;
              newn = y;
            }
        }
      end = newn;
      if (end == b)
        break;
    }
}

struct Jdb_mips_tlb : Jdb_module
{
  Jdb_mips_tlb() FIASCO_INIT;

  struct E
  {
    Unsigned16 index;
    Unsigned8  gid;
    Unsigned8  s;
    Mword      hi;
    Mword      lo[2];

    bool overlaps(E const &o) const
    {
      // different guest IDs
      if (gid != o.gid)
        return false;

      // different ASIDs
      if (gid == 0 && (!((lo[0] | o.lo[0]) & 1) && (hi & 0x3ff) != (o.hi & 0x3ff)))
        return false;

      Mword pm = ((1UL << s) - 1) | ((1UL << o.s) - 1);
      return (hi & ~pm) == (o.hi & ~pm);
    }

    void print() const
    {
      Mword pm = (1UL << s) - 1;
      printf("[%3u]: gid=%3x asid=%3lx va=%08lx pa=%08lx:%08lx ps=%lx attr=%02lx:%02lx\n",
             index, (unsigned)gid, hi & 0x3ff, hi & ~pm,
             (lo[0] << 6) & ~(pm >> 1),
             (lo[1] << 6) & ~(pm >> 1),
             (pm >> 1) + 1,
             lo[0] & 0x3f, lo[1] & 0x3f);
    }
  };


  static void dump_tlb(Cpu_number)
  {
    static E tlbs[1024];

    unsigned ntlb = Cpu::tlb_size() + Cpu::ftlb_sets() * Cpu::ftlb_ways();
    Mword old_entry_hi = Mem_unit::entry_hi();
    unsigned r = 0;
    for (unsigned i = 0; i < ntlb; ++i)
      {
        Mem_unit::index_reg(i);
        Mips::ehb();
        Mips::tlbr();
        Mips::ehb();
        Mword h = Mem_unit::entry_hi();
        if (h & (1UL << 10))
          // invalid entry, skip
          continue;

        Mword l0 = Mem_unit::entry_lo0();
        Mword l1 = Mem_unit::entry_lo1();

        Mword pm = Mips::mfc0_32(Mips::Cp0_page_mask);
        pm |= 0x1fff;

        Mword guest_id;
        asm volatile (ALTERNATIVE_INSN(
              "move %0, $zero",
              "mfc0 %0, $10, 4; ext %0, %0, 16, 8",
              0x4 /* VZ */)
            : "=r"(guest_id));

        unsigned ps = (sizeof(Mword) * 8) - __builtin_clz(pm);

        auto *t = &tlbs[r++];
        t->index = i;
        t->gid = guest_id;
        t->s = ps;
        t->hi = h;
        t->lo[0] = l0;
        t->lo[1] = l1;
      }

    sort(tlbs, tlbs + r, [](E const &l, E const &r)
        {
          if (l.gid < r.gid)
            return false;
          if (l.gid > r.gid)
            return true;

          unsigned lasid = 0xfff;

          if (!(l.lo[0] & 1))
            lasid = l.hi & 0x3ff;

          unsigned rasid = 0xfff;
          if (!(r.lo[0] & 1))
            rasid = r.hi & 0x3ff;

          if (l.gid != 0 || lasid == rasid)
            return (l.hi & ~0x3ffUL) > (r.hi & ~0x3ffUL);

          return lasid > rasid;
        });

    for (unsigned i = 0; i < r; ++i)
      {
        tlbs[i].print();
        for (unsigned x = 0; x < r; ++x)
          {
            if (x == i)
              continue;

            if (!tlbs[i].overlaps(tlbs[x]))
              continue;
            printf("  conflict: ");
            tlbs[x].print();
          }
      }

    Mem_unit::entry_hi(old_entry_hi);
    Mips::ehb();
  }

  Jdb_module::Action_code
  action(int cmd, void *&argbuf, char const *&fmt, int &next)
  {
    if (cmd != 0)
      return NOTHING;

    static Cpu_number cpu;
    if (argbuf != &cpu)
      {
        char const *c = (char const *)argbuf;
        if (*c == 'a')
          Jdb::on_each_cpu(dump_tlb);
        else if (*c >='0' && *c <= '9')
          {
            next = *c;
            argbuf = &cpu;
            fmt = "%i";
            return EXTRA_INPUT_WITH_NEXTCHAR;
          }
      }
    else if (!Cpu::online(cpu))
      printf("Error: CPU %d not online.\n", cxx::int_value<Cpu_number>(cpu));
    else
      Jdb::remote_work(cpu, dump_tlb);

    return NOTHING;
  }

  Jdb_module::Cmd const *cmds() const
  {
    static char c;
    static Cmd cs[] =
      {
        { 0, "x", "tlb", "%C", "x\tdump tlb entries",
          &c },
      };
    return cs;
  }

  int num_cmds() const
  { return 1; }
};

Jdb_mips_tlb::Jdb_mips_tlb()
  : Jdb_module("INFO")
{}

static Jdb_mips_tlb jdb_mips_tlb INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

}

