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
  NEW_TASK(s0);
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


static void print_node(Mapping* node, const Mapdb::Frame& frame, Mapping *sub = 0)
{
  assert (node);

  cout << "[UTEST] " << node->depth() << ": ";

  if (node == sub)
    cout << " ==> ";
  else
    cout << "     ";

  for (int d = node->depth(); d != 0; d--)
    cout << ' ';

  auto shift = frame.page_shift();

  cout << "space="  << *node->space()
       << " vaddr=0x" << (node->page() << shift)
       << " size=0x" << (Mapdb::Pfn(1) << shift)
       << endl;

  Mapdb::foreach_mapping(frame, node, Mapdb::Pfn(0), Mapdb::Pfn(~0),
      [sub](Mapping *node, Mapdb::Order shift)
      {
        cout << "[UTEST] " << node->depth() << ": ";
        if (node == sub)
          cout << " ==> ";
        else
          cout << "     ";
        for (int d = node->depth(); d != 0; d--)
          cout << ' ';

        cout << "space="  << *node->space()
             << " vaddr=0x" << (node->pfn(shift))
             << " size=0x" << (Mapdb::Pfn(1) << shift);

        if (Mapping* p = node->parent())
          {
            cout << " parent=" << *p->space()
                 << " p.vaddr=0x" << p->pfn(shift);
          }

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
  Mapping_tree* t = f->tree.get();

  if (! t)
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

  cout << setbase(10)
       << "[UTEST] " << tree_ind << "mapping tree: { " << *t->mappings()[0].space()
       << " va=" << pages->vaddr(t->mappings())
       << " size=" << (Mapdb::Pfn(1) << pages->page_shift()) << endl;

  tree_ind = std::string(indent + 2, ' ');

  cout << "[UTEST] " << tree_ind << "header info: entries used: " << t->_count
       << " free: " << t->_empty_count
       << " total: " << t->number_of_entries()
       << " lock: " << f->lock.test() << endl;

  if (unsigned (t->_count) + t->_empty_count > t->number_of_entries())
    {
      cout << "[UTEST] " << tree_ind << "seems to be corrupt tree..." << endl;
      return;
    }

  for (Mapping* m = t->mappings(); m != t->end(); m++)
    {
      if (m->depth() == Mapping::Depth_empty)
        continue;

      cout << "[UTEST] " << tree_ind << (m - t->mappings() +1) << ": ";

      if (m->depth() == Mapping::Depth_submap)
        cout << "subtree..." << endl;
      else
        {
          if (m->depth() == Mapping::Depth_end)
            {
              cout << "end" << endl;
              break;
            }
          else
            {
              ind = std::string(m->depth() * 2, ' ');
              cout << ind << "va=" << pages->vaddr(m) << " task=" << *m->space()
                   << " depth=";

              if (m->depth() == Mapping::Depth_root)
                cout << "root" << endl;
              else
                cout << m->depth() << endl;
            }
        }

      if (m->depth() == Mapping::Depth_submap)
        for (Mapping::Pcnt subo = Mapping::Pcnt(0);
             cxx::mask_lsb(subo,  pages->page_shift()) == Mapping::Pcnt(0);
             subo += (Mapping::Pcnt(1) << m->submap()->page_shift()))
          show_tree(m->submap(), subo /*cxx::get_lsb(offset, pages->page_shift())*/, base_size, subtree + 1, tree_ind.size() + ind.size());
    }

  tree_ind = std::string(indent, ' ');

  cout << "[UTEST] " << tree_ind << "} // mapping tree: " << *t->mappings()[0].space()
       << " va=" << pages->vaddr(t->mappings()) << endl;

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
  Mapping *node, *sub;
  Mapdb::Frame frame;

  init_test("basic");

  TEST (! m.lookup(other, to_pfn(Config::PAGE_SIZE),
                     to_pfn(Config::PAGE_SIZE),
                     &node, &frame));

  cout << "[UTEST] Looking up 4M node at physaddr=0K" << endl;
  TEST (m.lookup(s0,
                   to_pfn(0),
                   to_pfn(0),
                   &node, &frame));
  print_node(node, frame);

  cout << "[UTEST] Inserting submapping" << endl;
  sub = m.insert(frame, node, other,
                 to_pfn(2 * Config::PAGE_SIZE),
                 to_pfn(Config::PAGE_SIZE),
                 to_pcnt(Config::PAGE_SHIFT));
  TEST (sub);
  print_node(node, frame, sub);
  m.free(frame);

  //////////////////////////////////////////////////////////////////////

  cout << "[UTEST] Looking up 4M node at physaddr=8M" << endl;
  TEST (m.lookup(s0,
                   to_pfn(2 * Config::SUPERPAGE_SIZE),
                   to_pfn(2 * Config::SUPERPAGE_SIZE),
                   &node, &frame));
  print_node(node, frame);

  // XXX broken mapdb: assert (node->size() == Config::SUPERPAGE_SIZE);

  cout << "[UTEST] Inserting submapping" << endl;
  sub = m.insert(frame, node, other,
                 to_pfn(4 * Config::SUPERPAGE_SIZE),
                 to_pfn(2 * Config::SUPERPAGE_SIZE),
                 to_pcnt(Config::SUPERPAGE_SHIFT));
  print_node(node, frame, sub);

  // Before we can insert new mappings, we must free the tree.
  m.free(frame);

  cout << "[UTEST] Get that mapping again" << endl;
  TEST (m.lookup(other,
                   to_pfn(4 * Config::SUPERPAGE_SIZE),
                   to_pfn(2 * Config::SUPERPAGE_SIZE),
                   &sub, &frame));
  print_node(sub, frame);
  TEST (m.shift(frame, sub) == Mapdb::Order(Config::SUPERPAGE_SHIFT - Config::PAGE_SHIFT));

  node = sub->parent();

  cout << "[UTEST] Inserting 4K submapping" << endl;
  TEST ( m.insert(frame, sub, client,
                    to_pfn(15 * Config::PAGE_SIZE),
                    to_pfn(2 * Config::SUPERPAGE_SIZE),
                    to_pcnt(Config::PAGE_SHIFT)));
  print_node(node, frame);

  m.free(frame);

  test_done();
}

static void print_whole_tree(Mapping *node, const Mapdb::Frame& frame)
{
  while(node->parent())
    node = node->parent();
  print_node (node, frame);
}


static void maphole(Mapdb &m)
{
  Mapping *gf_map, *f_map, *son_map, *daughter_map;
  Mapdb::Frame frame;

  init_test("maphole");

  cout << "[UTEST] Looking up 4K node at physaddr=0" << endl;
  TEST (m.lookup (grandfather, to_pfn(0),
		    to_pfn(0), &gf_map, &frame));
  print_whole_tree (gf_map, frame);

  cout << "[UTEST] Inserting father mapping" << endl;
  f_map = m.insert (frame, gf_map, father, to_pfn(0),
		    to_pfn(0),
		    to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (gf_map, frame);
  m.free(frame);


  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup (father, to_pfn(0),
		    to_pfn(0), &f_map, &frame));
  print_whole_tree (f_map, frame);

  cout << "[UTEST] Inserting son mapping" << endl;
  son_map = m.insert (frame, f_map, son, to_pfn(0),
		      to_pfn(0),
		      to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (f_map, frame);
  m.free(frame);


  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup (father, to_pfn(0),
		    to_pfn(0), &f_map, &frame));
  print_whole_tree (f_map, frame);

  cout << "[UTEST] Inserting daughter mapping" << endl;
  daughter_map = m.insert (frame, f_map, daughter, to_pfn(0),
			   to_pfn(0),
			   to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (f_map, frame);
  m.free(frame);


  cout << "[UTEST] Looking up son at physaddr=0" << endl;
  TEST (m.lookup(son, to_pfn(0),
		   to_pfn(0), &son_map, &frame));
  f_map = son_map->parent();
  print_whole_tree (son_map, frame);
  show_tree(m.dbg_treemap());

  cout << "[UTEST] Son has accident on return from disco" << endl;
  m.flush(frame, son_map, L4_map_mask::full(),
	  to_pfn(0),
	  to_pfn(Config::PAGE_SIZE));
  m.free(frame);
  show_tree(m.dbg_treemap());

  cout << "[UTEST] Lost aunt returns from holidays" << endl;
  TEST (m.lookup (grandfather, to_pfn(0),
		    to_pfn(0), &gf_map, &frame));
  print_whole_tree (gf_map, frame);

  cout << "[UTEST] Inserting aunt mapping" << endl;
  TEST (m.insert (frame, gf_map, aunt, to_pfn(0),
		    to_pfn(0),
		    to_pcnt(Config::PAGE_SHIFT)));
  print_whole_tree (gf_map, frame);
  m.free(frame);
  show_tree(m.dbg_treemap());

  cout << "[UTEST] Looking up daughter at physaddr=0" << endl;
  TEST (m.lookup(daughter, to_pfn(0),
		   to_pfn(0), &daughter_map, &frame));
  print_whole_tree (daughter_map, frame);
  f_map = daughter_map->parent();
  cout << "[UTEST] Father of daugther is " << *f_map->space() << endl;

  TEST(f_map->space() == father);

  m.free(frame);
  test_done();
}


static void flushtest(Mapdb &m)
{
  Mapping *gf_map, *f_map;
  Mapdb::Frame frame;

  init_test("flushtest");

  cout << "[UTEST] Looking up 4K node at physaddr=0" << endl;
  TEST (m.lookup (grandfather, to_pfn(0), to_pfn(0), &gf_map, &frame));
  print_whole_tree (gf_map, frame);

  cout << "[UTEST] Inserting father mapping" << endl;
  f_map = m.insert (frame, gf_map, father, to_pfn(0), to_pfn(0), to_pcnt(Config::PAGE_SHIFT));
  print_whole_tree (gf_map, frame);
  m.free(frame);


  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup (father, to_pfn(0), to_pfn(0), &f_map, &frame));
  print_whole_tree (f_map, frame);

  cout << "[UTEST] Inserting son mapping" << endl;
  TEST (m.insert (frame, f_map, son, to_pfn(0), to_pfn(0), to_pcnt(Config::PAGE_SHIFT)));
  print_whole_tree (f_map, frame);
  m.free(frame);

  cout << "[UTEST] Lost aunt returns from holidays" << endl;
  TEST (m.lookup (grandfather, to_pfn(0), to_pfn(0), &gf_map, &frame));
  print_whole_tree (gf_map, frame);

  cout << "[UTEST] Inserting aunt mapping" << endl;
  TEST (m.insert (frame, gf_map, aunt, to_pfn(0), to_pfn(0), to_pcnt(Config::PAGE_SHIFT)));
  print_whole_tree (gf_map, frame);
  m.free(frame);

  cout << "[UTEST] Looking up father at physaddr=0" << endl;
  TEST (m.lookup(father, to_pfn(0), to_pfn(0), &f_map, &frame));
  gf_map = f_map->parent();
  print_whole_tree (gf_map, frame);

  cout << "[UTEST] father is killed by his new love" << endl;
  m.flush(frame, f_map, L4_map_mask::full(), to_pfn(0), to_pfn(Config::PAGE_SIZE));
  print_whole_tree (gf_map, frame);
  m.free(frame);

  cout << "[UTEST] Try resurrecting the killed father again" << endl;
  TEST (! m.lookup(father, to_pfn(0), to_pfn(0), &f_map, &frame));

  cout << "[UTEST] Resurrection is impossible, as it ought to be." << endl;

  test_done();
}


static Mapdb *new_multilevel_mapdb()
{
 static size_t page_sizes[] = { 30 - Config::PAGE_SHIFT,
      22 - Config::PAGE_SHIFT,
      21 - Config::PAGE_SHIFT,
      0};

  return new Mapdb(s0, Mapping::Page(1U << (42 - Config::PAGE_SHIFT - page_sizes[0])),
      page_sizes, sizeof(page_sizes) / sizeof(page_sizes[0]));
}


static Mapping *insert(Mapdb *m, Mapdb::Frame const &frame, Mapping *node, Fake_task *task,
                       Mapdb::Pfn virt, Mapdb::Pfn phys, Mapdb::Order order)
{
  Mapping *sub;
  std::cout << "[UTEST] Insert: " << *task << ": va=" << virt
            << " pa=" << phys
            << " order=" << order << std::endl;

  sub = m->insert(frame, node, task, virt, phys, Mapdb::Pcnt(1) << order);
  if (!sub)
    return 0;

  assert (task->insert(virt, phys, order));
  return sub;
}

static Fake_task::Entry const *
lookup(Mapdb *m, Fake_task *task, Mapdb::Pfn virt, Mapping **node,
       Mapdb::Frame *frame, bool verb = true)
{
  auto e = task->match(virt);
  if (!e)
    {
      if (verb)
        std::cout << "[UTEST] page-table lookup: " << virt << " in " << *task << " failed" << std::endl;
      return 0;
    }

  if (!m->lookup(task, virt, e->phys, node, frame))
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
  Mapping *node;
  Mapdb::Frame f;
  return lookup(m, task, virt, &node, &f, verb);
}

static bool map(Mapdb *m, Fake_task *from, Mapdb::Pfn fa, Fake_task *to, Mapdb::Pfn ta, Mapdb::Order order)
{
  Mapping *node, *sub;
  Mapdb::Frame f;
  auto e = lookup(m, from, fa, &node, &f);
  if (!e)
    {
      std::cout << "[UTEST] could not lookup source mapping: " << *from << " @ " << fa << std::endl;
      return false;
    }

  if (e->order < order)
    {
      m->free(f);
      std::cout << "[UTEST] could not create mapping (target size too large):"
                << " from " << *from << " @ " << fa << " order " << e->order
                << " -> to " << *to << " @ " << ta << " order " << order << std::endl;
      return false;
    }

  fa = cxx::mask_lsb(fa, order);

  if (cxx::get_lsb(ta, order) != Mapdb::Pcnt(0))
    {
      m->free(f);
      std::cout << "[UTEST] could not create mapping (misaligned):"
                << " from " << *from << " @ " << fa << " order " << e->order
                << " -> to " << *to << " @ " << ta << " order " << order << std::endl;
      return false;
    }

  if (!(sub = insert(m, f, node, to, ta, e->phys | cxx::get_lsb(fa, e->order), order)))
    {
      m->free(f);
      std::cout << "[UTEST] could not create mapping (MDB insert failed):"
                << " from " << *from << " @ " << fa << " order " << e->order
                << " -> to " << *to << " @ " << ta << " order " << order << std::endl;
      return false;
    }

  print_node(node, f, sub);
  m->free(f);
  return true;
}

static bool unmap(Mapdb *m, Fake_task *task, Mapdb::Pfn va_start, Mapdb::Pfn va_end, bool me_too)
{
  Mapping *node;
  Mapdb::Frame f;
  auto e = lookup(m, task, va_start, &node, &f);
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
  Mapdb::foreach_mapping(f, node, va_start, va_end, [](Mapping *m, Mapdb::Order size)
    {
      Fake_task *s = static_cast<Fake_task *>(m->space());
      Mapdb::Pfn page = m->pfn(size);
      std::cout << "[UTEST] unmap " << *s << " va=" << page << " for node:" << std::endl;
      s->remove(page);
    });

  m->flush(f, node, me_too ? L4_map_mask::full() : L4_map_mask(0), va_start, va_end);
  m->free(f);
  std::cout << "[UTEST] state after flush:" << std::endl;
  //print_node(node, f);

  return true;
}

void multilevel(Mapdb &m)
{
  Mapping *node, *sub;
  Mapdb::Frame frame, s0_frame;
  Mapdb::Pfn poffs = to_pfn(0xd2000000);

  init_test("multilevel MDB");

  cout << "[UTEST] Looking up 0xd2000000" << endl;
  TEST (lookup (&m, s0, to_pfn(0xd2000000), &node, &frame));

  s0_frame = frame;

  print_node (node, frame);

  cout << "[UTEST] Inserting submapping 4K" << endl;
  sub = insert(&m, s0_frame, node, other,
               to_pfn(2 * Config::PAGE_SIZE),
               poffs + to_pcnt(Config::PAGE_SHIFT),
               to_po(Config::PAGE_SHIFT));
  TEST (sub);
  print_node(node, s0_frame, sub);
  m.free(s0_frame);

  cout << "[UTEST] Get that mapping again" << endl;
  TEST (lookup(&m, other, to_pfn(2 * Config::PAGE_SIZE), &sub, &frame));

  print_node(sub, frame);
  TEST (m.shift(frame, sub) == Mapdb::Order(0));

  cout << "[UTEST] Inserting submapping 2M" << endl;
  sub = insert(&m, s0_frame, node, other,
               to_pfn(2 * (1 << 21)),
               poffs + to_pcnt(21),
               to_po(21));
  TEST (sub);
  print_node(node, s0_frame, sub);
  m.free(s0_frame);

  cout << "[UTEST] Get that mapping again" << endl;
  TEST (lookup(&m, other, to_pfn(2 * (1<<21)), &sub, &frame));

  print_node(sub, frame);
  TEST (m.shift(frame, sub) == Mapdb::Order(21 - Config::PAGE_SHIFT));


  cout << "[UTEST] Inserting submapping 2M" << endl;
  sub = insert(&m, s0_frame, node, aunt,
               to_pfn(1 << 21),
               poffs,
               to_po(21));
  TEST (sub);
  print_node(node, s0_frame, sub);
  m.free(s0_frame);

  TEST (map(&m, other, to_pfn(2 * (1<<21)), son, to_pfn(0xa0000000), to_po(12)));
  cout << "[UTEST] unamp from other..." << endl;
  TEST (unmap(&m, other, to_pfn(2 * (1<<21)), to_pfn(3 * (1<<21)), false));
  cout << "[UTEST] unamp from s0..." << endl;
  TEST (unmap(&m, s0, poffs + to_pcnt(21), poffs + to_pcnt(21) + to_pcnt(21), false));

  cout << "[UTEST] Map 8MB from s0 to father" << endl;
  TEST (map(&m, s0, to_pfn(0x51000000), father, to_pfn(0x3000000), to_po(22)));
  TEST (map(&m, s0, to_pfn(0x51400000), father, to_pfn(0x3400000), to_po(22)));

  cout << "[UTEST] Get first 8MB mapping" << endl;
  TEST (lookup(&m, father, to_pfn(0x3000000), &node, &frame));
  print_node(node, frame, sub);
  m.free(frame);

  cout << "[UTEST] Map 6MB from father to aunt" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, father, to_pfn(0x3200000 + (i << 21)), aunt, to_pfn(0x4000000 + (i<<21)), to_po(21)));

  cout << "[UTEST] Map 12K from aunt to client" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, aunt, to_pfn(0x4001000 + (i<<12)), client, to_pfn(0x2000 + (i << 12)), to_po(12)));

  TEST (!lookup(&m, client, to_pfn(0x1000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x2000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x3000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x4000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x5000), &node, &frame));

  TEST (unmap(&m, father, to_pfn(0x3202000), to_pfn(0x3203000), false));
  TEST (lookup(&m, father, to_pfn(0x3202000), &node, &frame));
  TEST (!lookup(&m, aunt, to_pfn(0x4000000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x1000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x2000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x3000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x4000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x5000), &node, &frame));

  cout << "[UTEST] Map 4MB from father to aunt" << endl;
  TEST (map(&m, father, to_pfn(0x3000000), aunt, to_pfn(0x5000000), to_po(22)));

  cout << "[UTEST] Map 12K from aunt to client" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, aunt, to_pfn(0x5201000 + (i<<12)), client, to_pfn(0x12000 + (i << 12)), to_po(12)));

  TEST (unmap(&m, aunt, to_pfn(0x5202000), to_pfn(0x5203000), false));
  TEST (lookup(&m, father, to_pfn(0x3202000), &node, &frame));
  TEST (lookup(&m, aunt, to_pfn(0x5200000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x11000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x12000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x13000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x14000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x15000), &node, &frame));

  cout << "[UTEST] Map 12K from aunt to client" << endl;
  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, s0, to_pfn(0x31ff000 + (i<<12)), client, to_pfn(0x22000 + (i << 12)), to_po(12)));

  TEST (lookup(&m, client, to_pfn(0x22000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x23000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x24000), &node, &frame));

  TEST (map(&m, s0, to_pfn(0x0), daughter, to_pfn(0x40000000), to_po(30)));
  TEST (!lookup(&m, daughter, to_pfn(0x3fffffff), &node, &frame));
  TEST (lookup(&m, daughter, to_pfn(0x50000000), &node, &frame));
  TEST (!lookup(&m, daughter, to_pfn(0x80000000), &node, &frame));

  for (unsigned i = 0; i < 3; ++i)
    TEST (map(&m, daughter, to_pfn(0x431ff000 + (i<<12)), client, to_pfn(0x32000 + (i<<12)), to_po(12)));

  TEST (map(&m, daughter, to_pfn(0x43000000), client, to_pfn(0x20000000), to_po(21)));
  TEST (map(&m, daughter, to_pfn(0x43200000), client, to_pfn(0x20200000), to_po(21)));

  TEST (lookup(&m, client, to_pfn(0x32000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x33000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x34000), &node, &frame));

  TEST (lookup(&m, client, to_pfn(0x20000000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x20200000), &node, &frame));

  TEST (map(&m, daughter, to_pfn(0x43000000), son, to_pfn(0x20000000), to_po(22)));
  TEST (map(&m, son, to_pfn(0x20200000), client, to_pfn(0x40000), to_po(12)));
  TEST (map(&m, son, to_pfn(0x20201000), client, to_pfn(0x41000), to_po(12)));

  TEST (lookup(&m, client, to_pfn(0x40000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x41000), &node, &frame));

  show_tree(m.dbg_treemap());
  TEST (unmap(&m, daughter, to_pfn(0x43200000), to_pfn(0x43201000), false));
  show_tree(m.dbg_treemap());

  TEST (!lookup(&m, client, to_pfn(0x40000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x41000), &node, &frame));

  TEST (lookup(&m, client, to_pfn(0x32000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x33000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x34000), &node, &frame));

  TEST (map(&m, daughter, to_pfn(0x43200000), client, to_pfn(0x33000), to_po(12)));
  TEST (lookup(&m, client, to_pfn(0x33000), &node, &frame));

  show_tree(m.dbg_treemap());
  TEST (unmap(&m, s0, to_pfn(0x3200000), to_pfn(0x3201000), false));

  TEST (lookup(&m, s0, to_pfn(0x3200000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x22000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x23000), &node, &frame));
  TEST (lookup(&m, client, to_pfn(0x24000), &node, &frame));

  TEST (!lookup(&m, client, to_pfn(0x32000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x33000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x34000), &node, &frame));

  TEST (!lookup(&m, client, to_pfn(0x20000000), &node, &frame));
  TEST (!lookup(&m, client, to_pfn(0x20200000), &node, &frame));

  TEST (!lookup(&m, daughter, to_pfn(0x40000000), &node, &frame));
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

  m = new Mapdb(s0, Mapping::Page(1U << (phys_bits - Config::PAGE_SHIFT - page_sizes[0])),
                page_sizes, page_sizes_max);
  basic(*m);

  m = new Mapdb(s0, Mapping::Page(1U << (phys_bits - Config::PAGE_SHIFT - page_sizes[0])),
                page_sizes, page_sizes_max);
  maphole(*m);

  m = new Mapdb(s0, Mapping::Page(1U << (phys_bits - Config::PAGE_SHIFT - page_sizes[0])),
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
