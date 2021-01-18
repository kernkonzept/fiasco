INTERFACE:
#include "mapdb.h"

IMPLEMENTATION:

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdlib>

#include "mapdb_i.h"

using namespace std;

#include "config.h"
#include "space.h"

typedef Virt_addr Phys_addr;

static size_t page_sizes[] =
{ Config::SUPERPAGE_SHIFT - Config::PAGE_SHIFT, 0 };

static size_t page_sizes_max = 2;

class Fake_factory : public Ram_quota
{
};

static inline
std::ostream &operator << (std::ostream &s, Mapdb::Pfn v)
{
  auto f = s.flags();
  s << setbase(16) << cxx::int_value<Mapdb::Pfn>(v);
  s.setf(f);
  return s;
}

static inline
std::ostream &operator << (std::ostream &s, Mapdb::Order v)
{
  auto f = s.flags();
  s << setbase(10) << cxx::int_value<Mapdb::Order>(v);
  s.setf(f);
  return s;
}

static inline
std::ostream &operator << (std::ostream &s, Mapping::Page v)
{
  auto f = s.flags();
  s << setbase(16) << cxx::int_value<Mapping::Page>(v);
  s.setf(f);
  return s;
}


class Fake_task : public Space
{
public:
  Fake_task(Ram_quota *r, char const *name)
  : Space(r, Caps::all()), name(name)
  {
    memset (tlb, 0, sizeof(tlb));
  }

  char const *name;

  struct Entry
  {
    Mapdb::Pfn virt;
    Mapdb::Pfn phys;
    Mapdb::Order order;
    bool valid;
  };

  enum { N_tlb = 12 };
  Entry tlb[N_tlb];

  Entry *match(Mapdb::Pfn virt)
  {
    for (Entry *e = tlb; e != &tlb[N_tlb]; ++e)
      {
        if (!e->valid)
          continue;

        if (cxx::mask_lsb(virt, e->order) == e->virt)
          return e;
      }
    return 0;
  }

  Entry const *match(Mapdb::Pfn virt) const
  {
    return const_cast<Fake_task *>(this)->match(virt);
  }

  void clear()
  {
    for (Entry *e = tlb; e != &tlb[N_tlb]; ++e)
      e->valid = false;
  }
};

PUBLIC
bool
Fake_task::insert(Mapdb::Pfn virt, Mapdb::Pfn phys, Mapdb::Order order)
{
  if (match(virt))
    {
      std::cout << "[UTEST] overmap: " << name << std::endl;
      return false;
    }

  for (Entry *e = tlb; e != &tlb[N_tlb]; ++e)
    {
      if (!e->valid)
        {
          std::cout <<
            "[UTEST] PT INSERT: " << name
            << " pa=" << phys << ":" << cxx::mask_lsb(phys, order)
            << " va=" << virt << ":" << cxx::mask_lsb(virt, order)
            << " order=" << order << std::endl;
          e->virt = cxx::mask_lsb(virt, order);
          e->phys = cxx::mask_lsb(phys, order);
          e->order = order;
          e->valid = true;
          return true;
        }
    }

  std::cout << "[UTEST] TLB overflow: " << name << std::endl;
  return false;
}

PUBLIC
bool
Fake_task::remove(Mapdb::Pfn virt)
{
  Entry *e = match(virt);
  if (!e || !e->valid)
    return false;

  std::cout << "[UTEST] remove PTE: " << name << " va=" << e->virt << " pa=" << e->phys
            << " order=" << e->order << std::endl;
  e->valid = false;
  return true;
}

static int total_tests_ok;
static int total_tests_failed;
static int __tests_ok;
static int __tests_failed;
static char const *__test_name;

static void init_test(char const *test)
{
  std::cout << "[UTEST] ========== " << test << " ======================================" << std::endl
            << "[UTEST] Initating test: " << test << std::endl;
  __tests_ok = 0;
  __tests_failed = 0;
  __test_name = test;
}

static void __test(bool ok, char const *txt)
{
  if (ok)
    ++__tests_ok;
  else
    ++__tests_failed;

  std::cout << "[UTEST] " << txt << ": \t" << (ok ? "OK" : "FAILED") << std::endl;
}

static void test_done()
{
  std::cout << "[UTEST] " << __test_name << " done: "
            << __tests_ok << " tests passed, " << __tests_failed << " failed" << std::endl
            << "[UTEST] " << __test_name << " " << (__tests_failed ? "FAILED" : "OK") << std::endl
            << "[UTEST] ---------- " << __test_name << " --------------------------------------" << std::endl;

  total_tests_ok += __tests_ok;
  total_tests_failed += __tests_failed;
  __test_name = "<NO TEST RUNNING>";
}

#define TEST(x) __test((x), #x)

class Sigma0_fake_task : public Fake_task
{
public:
  Sigma0_fake_task(Ram_quota *r, char const *name)
  : Fake_task(r, name) {}

  bool is_sigma0() const override { return true; }
};

static Fake_task *s0;
static Fake_task *other;
static Fake_task *client;
static Fake_task *father;
static Fake_task *&grandfather = s0;
static Fake_task *son;
static Fake_task *daughter;
static Fake_task *aunt;

static void init_spaces()
{
  static Fake_factory rq;
#define NEW_TASK(name) name = new Fake_task(&rq, #name)
  s0 = new Sigma0_fake_task(&rq, "s0");
  s0->insert(Mapdb::Pfn(0), Mapdb::Pfn(0), to_po(30));
  s0->insert(to_pfn(0x40000000), to_pfn(0x40000000), to_po(30));
  s0->insert(to_pfn(0x80000000), to_pfn(0x80000000), to_po(30));
  s0->insert(to_pfn(0xc0000000), to_pfn(0xc0000000), to_po(30));

  NEW_TASK(other);
  NEW_TASK(client);
  NEW_TASK(father);
  NEW_TASK(son);
  NEW_TASK(daughter);
  NEW_TASK(aunt);
#undef NEW_TASK
}
static
std::ostream &operator << (std::ostream &s, Space const &sp)
{
  Fake_task const *t = static_cast<Fake_task const *>(&sp);
  if (!t)
    s << "<NULL>";
  else
    s << t->name;
  return s;
}

static std::ostream &operator << (std::ostream &s, Fake_task::Entry const &e)
{
  if (!e.valid)
    s << "invalid entry";
  else
    s << "va=" << e.virt << " pa=" << e.phys << " order=" << e.order;
  return s;
}


static void print_node(const Mapdb::Frame& frame, Mapping *sub = 0)
{
  auto node = frame.m;
  int n_depth = 0;
  Space *n_space = frame.pspace();

  if (!*node)
    node = frame.frame->first();
  else
    n_depth = node->depth() + 1;

  cout << "[UTEST] " << n_depth << ": ";

  if (*frame.m && *node == sub)
    cout << " ==> ";
  else
    cout << "     ";

  for (int d = n_depth; d != 0; d--)
    cout << ' ';

  auto shift = frame.page_shift();

  cout << "space="  << *n_space
       << " vaddr=0x" << frame.pvaddr()
       << " size=0x" << (Mapdb::Pfn(1) << shift)
       << endl;

  Mapdb::foreach_mapping(frame, Mapdb::Pfn(0), Mapdb::Pfn(~0),
      [sub](Mapping *node, Mapdb::Order shift)
      {
        cout << "[UTEST] " << node->depth() + 1<< ": ";
        if (node == sub)
          cout << " ==> ";
        else
          cout << "     ";
        for (int d = node->depth() + 1; d != 0; d--)
          cout << ' ';

        cout << "space="  << *node->space()
             << " vaddr=0x" << (node->pfn(shift))
             << " size=0x" << (Mapdb::Pfn(1) << shift);

        cout << endl;
      });
  cout << "[UTEST] " << endl;
}


static void show_tree(Treemap *pages, Mapping::Pcnt offset = Mapping::Pcnt(0),
                      Mdb_types::Order base_size = Mapping::Order(0), int subtree = 0,
                      int indent = 0)
{
  typedef Treemap::Page Page;
  Page       page = pages->trunc_to_page(offset);
  Physframe*    f = pages->frame(page);

  if (!f->has_mappings())
    {
#if 0
      cout << setbase(10)
           << "[UTEST] T[" << subtree << "] no mapping tree registered for frame number "
           << setbase(16)
           << (page << pages->page_shift())
           << endl;
#endif
      return;
    }

  std::string ind = "";
  std::string tree_ind(indent, ' ');

  Mapping_tree* t = f->tree();
  auto frame_va = offset + pages->page_offset();



  cout << setbase(10)
       << "[UTEST] " << tree_ind << "mapping tree: { " << *pages->owner()
       << " va=" << frame_va
       << " size=" << (Mapdb::Pfn(1) << pages->page_shift()) << endl;

  tree_ind = std::string(indent + 2, ' ');

  cout << "[UTEST] " << tree_ind << "header info: lock: " << f->lock.test() << endl;

  for (auto m: *t)
    {
      cout << "[UTEST] " << tree_ind << ": ";

      if (m->submap())
        cout << "subtree..." << endl;
      else
        {
          int depth = m->depth() + 1;
          ind = std::string(depth * 2, ' ');
          cout << ind << "va=" << pages->vaddr(m) << " task=" << *m->space()
               << " depth=";

          if (depth == 0)
            cout << "root" << endl;
          else
            cout << depth << endl;
        }

      if (m->submap())
        for (Mapping::Pcnt subo = Mapping::Pcnt(0);
             cxx::mask_lsb(subo,  pages->page_shift()) == Mapping::Pcnt(0);
             subo += (Mapping::Pcnt(1) << m->submap()->page_shift()))
          show_tree(m->submap(), subo /*cxx::get_lsb(offset, pages->page_shift())*/, base_size, subtree + 1, tree_ind.size() + ind.size());
    }

  tree_ind = std::string(indent, ' ');

  cout << "[UTEST] " << tree_ind << "} // mapping tree: " << *pages->owner()
       << " va=" << frame_va << endl;

}

static
Mapdb::Pfn to_pfn(Address a)
{ return Mem_space::to_pfn(Mem_space::V_pfn(Virt_addr(a))); }

static
Mapdb::Pcnt to_pcnt(unsigned order)
{ return Mem_space::to_pcnt(Mem_space::Page_order(order)); }

static
Mapdb::Order to_po(unsigned order)
{ return Mapdb::Order(order - Config::PAGE_SHIFT); }



static void basic(Mapdb &m)
{
  Mapping *sub;
  Mapdb::Frame frame;

  init_test("basic");

  TEST (! m.lookup(other, to_pfn(Config::PAGE_SIZE),
                     to_pfn(Config::PAGE_SIZE),
                     &frame));

  cout << "[UTEST] Looking up 4M node at physaddr=0K" << endl;
  TEST (m.lookup(s0,
                   to_pfn(0),
                   to_pfn(0),
                   &frame));
  print_node(frame);

  cout << "[UTEST] Inserting submapping" << endl;
  sub = m.insert(frame, other,
                 to_pfn(2 * Config::PAGE_SIZE),
                 to_pfn(Config::PAGE_SIZE),
                 to_pcnt(Config::PAGE_SHIFT));
  TEST (sub);
  print_node(frame, sub);
  frame.clear();

  //////////////////////////////////////////////////////////////////////

  cout << "[UTEST] Looking up 4M node at physaddr=8M" << endl;
  TEST (m.lookup(s0,
                   to_pfn(2 * Config::SUPERPAGE_SIZE),
                   to_pfn(2 * Config::SUPERPAGE_SIZE),
                   &frame));
  print_node(frame);

  // XXX broken mapdb: assert (node->size() == Config::SUPERPAGE_SIZE);

  cout << "[UTEST] Inserting submapping" << endl;
  sub = m.insert(frame, other,
                 to_pfn(4 * Config::SUPERPAGE_SIZE),
                 to_pfn(2 * Config::SUPERPAGE_SIZE),
                 to_pcnt(Config::SUPERPAGE_SHIFT));
  print_node(frame, sub);
  auto parent_ma = frame.m;

  // Before we can insert new mappings, we must free the tree.
  frame.clear();

  cout << "[UTEST] Get that mapping again" << endl;
  TEST (m.lookup(other,
                   to_pfn(4 * Config::SUPERPAGE_SIZE),
                   to_pfn(2 * Config::SUPERPAGE_SIZE),
                   &frame));
  print_node(frame);
  TEST (frame.treemap->page_shift() == Mapdb::Order(Config::SUPERPAGE_SHIFT - Config::PAGE_SHIFT));

  cout << "[UTEST] Inserting 4K submapping" << endl;
  TEST ( m.insert(frame, client,
                    to_pfn(15 * Config::PAGE_SIZE),
                    to_pfn(2 * Config::SUPERPAGE_SIZE),
                    to_pcnt(Config::PAGE_SHIFT)));
  frame.m = parent_ma;
  print_node(frame);

  frame.clear();

  test_done();
}

static void print_whole_tree(const Mapdb::Frame& frame)
{
  auto f = frame;
  f.m = Mapping_tree::Iterator(); //f.frame->first();
  print_node(f);
}


static void maphole(Mapdb &m)
{
  Mapdb::Frame frame;

  init_test("maphole");

  cout << "[UTEST] Looking up 4K node at physaddr=0" << endl;
  TEST (m.lookup (grandfather, to_pfn(0),
		    to_pfn(0), &frame));
  print_whole_tree (frame);

  cout << "[UTEST] Inserting father mapping" << endl;
  m.insert (frame, father, to_pfn(0),
		    to_pfn(0),
		    to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (frame);
  frame.clear();


  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup (father, to_pfn(0),
		    to_pfn(0), &frame));
  print_whole_tree (frame);

  cout << "[UTEST] Inserting son mapping" << endl;
  m.insert (frame, son, to_pfn(0),
		      to_pfn(0),
		      to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (frame);
 frame.clear();


  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup (father, to_pfn(0),
		    to_pfn(0), &frame));
  print_whole_tree (frame);

  cout << "[UTEST] Inserting daughter mapping" << endl;
  m.insert (frame, daughter, to_pfn(0),
			   to_pfn(0),
			   to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (frame);
  frame.clear();


  cout << "[UTEST] Looking up son at physaddr=0" << endl;
  TEST (m.lookup(son, to_pfn(0),
		   to_pfn(0), &frame));
  print_whole_tree (frame);
  show_tree(m.dbg_treemap());

  cout << "[UTEST] Son has accident on return from disco" << endl;
  m.flush(frame, L4_map_mask::full(),
	  to_pfn(0),
	  to_pfn(Config::PAGE_SIZE));
  frame.clear();
  show_tree(m.dbg_treemap());

  cout << "[UTEST] Lost aunt returns from holidays" << endl;
  TEST (m.lookup (grandfather, to_pfn(0),
		    to_pfn(0), &frame));
  print_whole_tree (frame);

  cout << "[UTEST] Inserting aunt mapping" << endl;
  TEST (m.insert (frame, aunt, to_pfn(0),
		    to_pfn(0),
		    to_pcnt(Config::PAGE_SHIFT)));
  print_whole_tree (frame);
  frame.clear();
  show_tree(m.dbg_treemap());

  cout << "[UTEST] Looking up daughter at physaddr=0" << endl;
  TEST (m.lookup(daughter, to_pfn(0),
		   to_pfn(0), &frame));
  print_whole_tree (frame);
  //cout << "[UTEST] Father of daugther is " << *frame.m->parent()->space() << endl;

  //TEST(frame.m->parent()->space() == father);

  frame.clear();
  test_done();
}


static void flushtest(Mapdb &m)
{
  Mapdb::Frame frame;

  init_test("flushtest");

  cout << "[UTEST] Looking up 4K node at physaddr=0" << endl;
  TEST (m.lookup (grandfather, to_pfn(0), to_pfn(0), &frame));
  print_whole_tree (frame);

  cout << "[UTEST] Inserting father mapping" << endl;
  m.insert (frame, father, to_pfn(0), to_pfn(0), to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (frame);
  frame.clear();


  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup (father, to_pfn(0), to_pfn(0), &frame));
  print_whole_tree (frame);

  cout << "[UTEST] Inserting son mapping" << endl;
  TEST (m.insert (frame, son, to_pfn(0), to_pfn(0), to_pcnt(Config::PAGE_SHIFT)));
  print_whole_tree (frame);
  frame.clear();

  cout << "[UTEST] Lost aunt returns from holidays" << endl;
  TEST (m.lookup (grandfather, to_pfn(0), to_pfn(0), &frame));
  print_whole_tree (frame);

  cout << "[UTEST] Inserting aunt mapping" << endl;
  TEST (m.insert (frame, aunt, to_pfn(0), to_pfn(0), to_pcnt(Config::PAGE_SHIFT)));
  print_whole_tree (frame);
  frame.clear();

  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup(father, to_pfn(0), to_pfn(0), &frame));

  print_whole_tree (frame);

  cout << "[UTEST] father is killed by his new love" << endl;
  m.flush(frame, L4_map_mask::full(), to_pfn(0), to_pfn(Config::PAGE_SIZE));
  print_whole_tree (frame);
  frame.clear();

  cout << "[UTEST] Try resurrecting the killed father again" << endl;
  TEST (! m.lookup(father, to_pfn(0), to_pfn(0), &frame));

  cout << "[UTEST] Resurrection is impossible, as it ought to be." << endl;

  test_done();
}


static Mapdb *new_multilevel_mapdb()
{
 static size_t page_sizes[] = { 30 - Config::PAGE_SHIFT,
      22 - Config::PAGE_SHIFT,
      21 - Config::PAGE_SHIFT,
      0};

  return new Mapdb(s0, Mapping::Order(42 - Config::PAGE_SHIFT),
      page_sizes, sizeof(page_sizes) / sizeof(page_sizes[0]));
}


static Mapping *insert(Mapdb *m, Mapdb::Frame const &frame, Fake_task *task,
                       Mapdb::Pfn virt, Mapdb::Pfn phys, Mapdb::Order order)
{
  Mapping *sub;
  std::cout << "[UTEST] Insert: " << *task << ": va=" << virt
            << " pa=" << phys
            << " order=" << order << std::endl;

  sub = m->insert(frame, task, virt, phys, Mapdb::Pcnt(1) << order);
  if (!sub)
    return 0;

  assert (task->insert(virt, phys, order));
  return sub;
}

static Fake_task::Entry const *
lookup(Mapdb *m, Fake_task *task, Mapdb::Pfn virt,
       Mapdb::Frame *frame, bool verb = true)
{
  auto e = task->match(virt);
  if (!e)
    {
      if (verb)
        std::cout << "[UTEST] page-table lookup: " << virt << " in " << *task << " failed" << std::endl;
      return 0;
    }

  if (!m->lookup(task, virt, e->phys, frame))
    {
      if (verb)
        std::cout << "[UTEST] MDB lookup: " << virt << " in " << *task << ": " << *e << ": failed" << std::endl;
      return 0;
    }

  return e;
}

static Fake_task::Entry const *
lookup(Mapdb *m, Fake_task *task, Mapdb::Pfn virt, bool verb = true)
{
  Mapdb::Frame f;
  return lookup(m, task, virt, &f, verb);
}

static bool map(Mapdb *m, Fake_task *from, Mapdb::Pfn fa, Fake_task *to, Mapdb::Pfn ta, Mapdb::Order order)
{
  Mapping *sub;
  Mapdb::Frame f;
  auto e = lookup(m, from, fa, &f);
  if (!e)
    {
      std::cout << "[UTEST] could not lookup source mapping: " << *from << " @ " << fa << std::endl;
      return false;
    }

  if (e->order < order)
    {
      f.clear();
      std::cout << "[UTEST] could not create mapping (target size too large):"
                << " from " << *from << " @ " << fa << " order " << e->order
                << " -> to " << *to << " @ " << ta << " order " << order << std::endl;
      return false;
    }

  fa = cxx::mask_lsb(fa, order);

  if (cxx::get_lsb(ta, order) != Mapdb::Pcnt(0))
    {
      f.clear();
      std::cout << "[UTEST] could not create mapping (misaligned):"
                << " from " << *from << " @ " << fa << " order " << e->order
                << " -> to " << *to << " @ " << ta << " order " << order << std::endl;
      return false;
    }

  if (!(sub = insert(m, f, to, ta, e->phys | cxx::get_lsb(fa, e->order), order)))
    {
      f.clear();
      std::cout << "[UTEST] could not create mapping (MDB insert failed):"
                << " from " << *from << " @ " << fa << " order " << e->order
                << " -> to " << *to << " @ " << ta << " order " << order << std::endl;
      return false;
    }

  print_node(f, sub);
  f.clear();
  return true;
}

static bool unmap(Mapdb *m, Fake_task *task, Mapdb::Pfn va_start, Mapdb::Pfn va_end, bool me_too)
{
  Mapdb::Frame f;
  auto e = lookup(m, task, va_start, &f);
  if (!e)
    {
      std::cout << "[UTEST] could not lookup mapping: " << *task << " @ " << va_start << std::endl;
      return false;
    }

  if (cxx::mask_lsb(va_start, e->order) != cxx::mask_lsb(va_end - Mapdb::Pcnt(1), e->order))
    {
      std::cout << "[UTEST] va range (" << va_start << "-" << va_end << ") "
                << "must be in the original page (order=" << e->order << ")" << std::endl;
      return false;
    }

  if (me_too)
    task->remove(va_start);

  // now delete from the other address spaces
  Mapdb::foreach_mapping(f, va_start, va_end, [](Mapping *m, Mapdb::Order size)
    {
      Fake_task *s = static_cast<Fake_task *>(m->space());
      Mapdb::Pfn page = m->pfn(size);
      std::cout << "[UTEST] unmap " << *s << " va=" << page << " for node:" << std::endl;
      s->remove(page);
    });

  m->flush(f, me_too ? L4_map_mask::full() : L4_map_mask(0), va_start, va_end);
  //print_node(f);
  f.clear();
  std::cout << "[UTEST] state after flush:" << std::endl;

  return true;
}

void multilevel(Mapdb &m)
{
  Mapdb::Frame frame, s0_frame;
  Mapdb::Pfn poffs = to_pfn(0xd2000000);

  init_test("multilevel MDB");

  cout << "[UTEST] Looking up 0xd2000000" << endl;
  TEST (lookup (&m, s0, to_pfn(0xd2000000), &frame));

  s0_frame = frame;

  print_node (frame);

  cout << "[UTEST] Inserting submapping 4K" << endl;
  Mapping *sub = insert(&m, s0_frame, other,
                        to_pfn(2 * Config::PAGE_SIZE),
                        poffs + to_pcnt(Config::PAGE_SHIFT),
                        to_po(Config::PAGE_SHIFT));
  TEST (sub);
  print_node(s0_frame, sub);
  //s0_frame.clear(true);

  cout << "[UTEST] Get that mapping again" << endl;
  TEST (lookup(&m, other, to_pfn(2 * Config::PAGE_SIZE), &frame));

  print_node(frame);
  TEST (frame.treemap->page_shift() == Mapdb::Order(0));

  cout << "[UTEST] Inserting submapping 2M" << endl;
  sub = insert(&m, s0_frame, other,
               to_pfn(2 * (1 << 21)),
               poffs + to_pcnt(21),
               to_po(21));
  TEST (sub);
  print_node(s0_frame, sub);
  //s0_frame.clear(true);

  cout << "[UTEST] Get that mapping again" << endl;
  TEST (lookup(&m, other, to_pfn(2 * (1<<21)), &frame));

  print_node(frame);
  TEST (frame.treemap->page_shift() == Mapdb::Order(21 - Config::PAGE_SHIFT));


  cout << "[UTEST] Inserting submapping 2M" << endl;
  sub = insert(&m, s0_frame, aunt,
               to_pfn(1 << 21),
               poffs,
               to_po(21));
  TEST (sub);
  print_node(s0_frame, sub);
  s0_frame.clear();

  TEST (map(&m, other, to_pfn(2 * (1<<21)), son, to_pfn(0xa0000000), to_po(12)));
  cout << "[UTEST] unamp from other..." << endl;
  TEST (unmap(&m, other, to_pfn(2 * (1<<21)), to_pfn(3 * (1<<21)), false));
  cout << "[UTEST] unamp from s0..." << endl;
  TEST (unmap(&m, s0, poffs + to_pcnt(21), poffs + to_pcnt(21) + to_pcnt(21), false));

  cout << "[UTEST] Map 8MB from s0 to father" << endl;
  TEST (map(&m, s0, to_pfn(0x51000000), father, to_pfn(0x3000000), to_po(22)));
  TEST (map(&m, s0, to_pfn(0x51400000), father, to_pfn(0x3400000), to_po(22)));

  sub = *frame.m;
  cout << "[UTEST] Get first 8MB mapping" << endl;
  TEST (lookup(&m, father, to_pfn(0x3000000), &frame));
  print_node(frame, sub);
  frame.clear();

  cout << "[UTEST] Map 6MB from father to aunt" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, father, to_pfn(0x3200000 + (i << 21)), aunt, to_pfn(0x4000000 + (i<<21)), to_po(21)));

  cout << "[UTEST] Map 12K from aunt to client" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, aunt, to_pfn(0x4001000 + (i<<12)), client, to_pfn(0x2000 + (i << 12)), to_po(12)));

  TEST (!lookup(&m, client, to_pfn(0x1000), &frame));
  TEST (lookup(&m, client, to_pfn(0x2000), &frame));
  TEST (lookup(&m, client, to_pfn(0x3000), &frame));
  TEST (lookup(&m, client, to_pfn(0x4000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x5000), &frame));

  TEST (unmap(&m, father, to_pfn(0x3202000), to_pfn(0x3203000), false));
  TEST (lookup(&m, father, to_pfn(0x3202000), &frame));
  TEST (!lookup(&m, aunt, to_pfn(0x4000000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x1000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x2000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x3000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x4000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x5000), &frame));

  cout << "[UTEST] Map 4MB from father to aunt" << endl;
  TEST (map(&m, father, to_pfn(0x3000000), aunt, to_pfn(0x5000000), to_po(22)));

  cout << "[UTEST] Map 12K from aunt to client" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, aunt, to_pfn(0x5201000 + (i<<12)), client, to_pfn(0x12000 + (i << 12)), to_po(12)));

  TEST (unmap(&m, aunt, to_pfn(0x5202000), to_pfn(0x5203000), false));
  TEST (lookup(&m, father, to_pfn(0x3202000), &frame));
  TEST (lookup(&m, aunt, to_pfn(0x5200000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x11000), &frame));
  TEST (lookup(&m, client, to_pfn(0x12000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x13000), &frame));
  TEST (lookup(&m, client, to_pfn(0x14000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x15000), &frame));

  cout << "[UTEST] Map 12K from aunt to client" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, s0, to_pfn(0x31ff000 + (i<<12)), client, to_pfn(0x22000 + (i << 12)), to_po(12)));

  TEST (lookup(&m, client, to_pfn(0x22000), &frame));
  TEST (lookup(&m, client, to_pfn(0x23000), &frame));
  TEST (lookup(&m, client, to_pfn(0x24000), &frame));

  TEST (map(&m, s0, to_pfn(0x0), daughter, to_pfn(0x40000000), to_po(30)));
  TEST (!lookup(&m, daughter, to_pfn(0x3fffffff), &frame));
  TEST (lookup(&m, daughter, to_pfn(0x50000000), &frame));
  TEST (!lookup(&m, daughter, to_pfn(0x80000000), &frame));

  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, daughter, to_pfn(0x431ff000 + (i<<12)), client, to_pfn(0x32000 + (i<<12)), to_po(12)));

  TEST (map(&m, daughter, to_pfn(0x43000000), client, to_pfn(0x20000000), to_po(21)));
  TEST (map(&m, daughter, to_pfn(0x43200000), client, to_pfn(0x20200000), to_po(21)));

  TEST (lookup(&m, client, to_pfn(0x32000), &frame));
  TEST (lookup(&m, client, to_pfn(0x33000), &frame));
  TEST (lookup(&m, client, to_pfn(0x34000), &frame));

  TEST (lookup(&m, client, to_pfn(0x20000000), &frame));
  TEST (lookup(&m, client, to_pfn(0x20200000), &frame));

  TEST (map(&m, daughter, to_pfn(0x43000000), son, to_pfn(0x20000000), to_po(22)));
  TEST (map(&m, son, to_pfn(0x20200000), client, to_pfn(0x40000), to_po(12)));
  TEST (map(&m, son, to_pfn(0x20201000), client, to_pfn(0x41000), to_po(12)));

  TEST (lookup(&m, client, to_pfn(0x40000), &frame));
  TEST (lookup(&m, client, to_pfn(0x41000), &frame));

  show_tree(m.dbg_treemap());
  TEST (unmap(&m, daughter, to_pfn(0x43200000), to_pfn(0x43201000), false));
  show_tree(m.dbg_treemap());

  TEST (!lookup(&m, client, to_pfn(0x40000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x41000), &frame));

  TEST (lookup(&m, client, to_pfn(0x32000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x33000), &frame));
  TEST (lookup(&m, client, to_pfn(0x34000), &frame));

  TEST (map(&m, daughter, to_pfn(0x43200000), client, to_pfn(0x33000), to_po(12)));
  TEST (lookup(&m, client, to_pfn(0x33000), &frame));

  show_tree(m.dbg_treemap());
  TEST (unmap(&m, s0, to_pfn(0x3200000), to_pfn(0x3201000), false));

  TEST (lookup(&m, s0, to_pfn(0x3200000), &frame));
  TEST (lookup(&m, client, to_pfn(0x22000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x23000), &frame));
  TEST (lookup(&m, client, to_pfn(0x24000), &frame));

  TEST (!lookup(&m, client, to_pfn(0x32000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x33000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x34000), &frame));

  TEST (!lookup(&m, client, to_pfn(0x20000000), &frame));
  TEST (!lookup(&m, client, to_pfn(0x20200000), &frame));

  TEST (!lookup(&m, daughter, to_pfn(0x40000000), &frame));
  show_tree(m.dbg_treemap());

  test_done();
}


void multilevel2(Mapdb &m)
{
  client->clear();
  son->clear();
  father->clear();
  daughter->clear();

  init_test("multilevel 2");

  TEST (map(&m, s0, to_pfn(0x40000000), father, to_pfn(0x80000000), to_po(30)));
  TEST (map(&m, father, to_pfn(0x80000000), son, to_pfn(0x0), to_po(21)));

  TEST (lookup(&m, son, to_pfn(0x0)));
  TEST (unmap(&m, s0, to_pfn(0x40400000), to_pfn(0x40401000), false));
  TEST (!lookup(&m, son, to_pfn(0x0)));

  test_done();
};

void multilevel3(Mapdb &m)
{
  client->clear();
  son->clear();
  father->clear();
  daughter->clear();

  init_test("multilevel 3");

  TEST (map(&m, s0, to_pfn(0x00000000), father, to_pfn(0x00000000), to_po(30)));
  TEST (map(&m, father, to_pfn(0x00000000), son, to_pfn(0x80200000), to_po(21)));

  TEST (lookup(&m, son, to_pfn(0x80200000)));
  TEST (unmap(&m, s0, to_pfn(0x00000000), to_pfn(0x00001000), false));
  TEST (!lookup(&m, son, to_pfn(0x80200000)));

  test_done();
};




#include "boot_info.h"
#include "cpu.h"
#include "config.h"
#include "kip_init.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "per_cpu_data_alloc.h"
#include "static_init.h"
#include "usermode.h"

class Timeout;

DEFINE_PER_CPU Per_cpu<Timeout *> timeslice_timeout;
STATIC_INITIALIZER_P(init, STARTUP_INIT_PRIO);
STATIC_INITIALIZER_P(init2, POST_CPU_LOCAL_INIT_PRIO);

static void init()
{
  Usermode::init(Cpu_number::boot_cpu());
  Boot_info::init();
  Kip_init::init();
  Kmem_alloc::base_init();
  Kmem_alloc::init();

  // Initialize cpu-local data management and run constructors for CPU 0
  Per_cpu_data::init_ctors();
  Per_cpu_data_alloc::alloc(Cpu_number::boot_cpu());
  Per_cpu_data::run_ctors(Cpu_number::boot_cpu());

}

static void init2()
{
  Cpu::init_global_features();
  Config::init();
  Kmem::init_mmu(Cpu::cpus.cpu(Cpu_number::boot_cpu()));
}

int main()
{
  init_spaces();

  unsigned phys_bits = 32;
  Mapdb *m;

  m = new Mapdb(s0, Mapping::Order(phys_bits - Config::PAGE_SHIFT),
                page_sizes, page_sizes_max);
  basic(*m);

  m = new Mapdb(s0, Mapping::Order(phys_bits - Config::PAGE_SHIFT),
                page_sizes, page_sizes_max);
  maphole(*m);

  m = new Mapdb(s0, Mapping::Order(phys_bits - Config::PAGE_SHIFT),
                page_sizes, page_sizes_max);
  flushtest(*m);

  basic(*new_multilevel_mapdb());
  maphole(*new_multilevel_mapdb());
  flushtest(*new_multilevel_mapdb());
  multilevel(*new_multilevel_mapdb());

  multilevel2(*new_multilevel_mapdb());
  multilevel3(*new_multilevel_mapdb());

  std::cout << "[UTEST] #################################################################" << std::endl
            << "[UTEST] All tests finished: "
            << total_tests_ok << " tests passed, " << total_tests_failed << " failed" << std::endl;
  return(0);
}
