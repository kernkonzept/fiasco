IMPLEMENTATION:

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdlib>

using namespace std;

#include "map_util.h"
#include "space.h"
#include "globals.h"
#include "config.h"

#include "boot_info.h"
#include "cpu.h"
#include "config.h"
#include "kip_init.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "per_cpu_data_alloc.h"
#include "pic.h"
#include "static_init.h"
#include "usermode.h"
#include "vmem_alloc.h"

IMPLEMENTATION:

typedef L4_fpage Test_fpage;

class Test_space : public Space
{
public:
  Test_space(Ram_quota *rq, char const *name)
  : Space(rq, Caps::all()), name(name)
  {
    initialize();
  }

  void* operator new (size_t s)
  { return malloc (s); }

  void operator delete (void *p)
  { ::free (p); }

  char const *const name;
};

class Sigma0_space : public Test_space
{
public:
  explicit Sigma0_space(Ram_quota *q) : Test_space(q, "s0") {}
  bool is_sigma0() const { return true; }
};


PUBLIC
bool
Sigma0_space::v_fabricate(Mem_space::Vaddr address,
                          Mem_space::Phys_addr *phys, Mem_space::Page_order *size,
                          Mem_space::Attr *attribs = 0)
{
  // special-cased because we don't do ptab lookup for sigma0
  *size = static_cast<Mem_space const &>(*this).largest_page_size();
  *phys = cxx::mask_lsb(Virt_addr(address), *size);

  if (attribs)
    *attribs = Mem_space::Attr(L4_fpage::Rights::URWX());

  return true;
}

PUBLIC inline
Page_number
Sigma0_space::mem_space_map_max_address() const
{ return Page_number(1UL << (MWORD_BITS - Mem_space::Page_shift)); }

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

  Mem_space::init_page_sizes();
}

static void init2()
{
  Cpu::init_global_features();
  Config::init();
  Kmem::init_mmu(*Cpu::boot_cpu());
}

class Fake_factory : public Ram_quota
{
};

extern Static_object<Mapdb> mapdb_mem;
static Fake_factory rq;

static
Mem_space *ms(Space *s) { return s; }

static
Mem_space::Vaddr
to_vaddr(Address a)
{ return Mem_space::Vaddr(Virt_addr(a)); }

static
Mapdb::Pfn to_pfn(Address a)
{ return Mem_space::to_pfn(Virt_addr(a)); }

int main()
{
  cout << "[UTEST] *** Create tasks ***" << endl;

  Space *sigma0 = new Sigma0_space(&rq);

  init_mapdb_mem(sigma0);

  Test_space *server = new Test_space (&rq, "server");
  assert (server);
  Test_space *client = new Test_space (&rq, "client");
  assert (client);
  Test_space *client2 = new Test_space (&rq, "client2");
  assert (client2);

  cout << "[UTEST] *** Manipulate mappings ***" << endl;

  Mapdb* mapdb = mapdb_mem.get();

  typedef Mem_space::Page_order Page_order;
  typedef Mem_space::Phys_addr Phys_addr;
  typedef Mem_space::Attr Attr;;

  Phys_addr phys;
  Page_order order;
  Attr page_attribs;
  Mapping *m;
  Mapdb::Frame frame;

  cout << "[UTEST] s0 [0x10000] -> server [0x1000]" << endl;

  assert (ms(server)->v_lookup(to_vaddr(0x1000), &phys, &order, &page_attribs) 
	  == false);
  Kobject::Reap_list reap;
  fpage_map(sigma0,
	    Test_fpage::mem(0x10000, Config::PAGE_SHIFT, L4_fpage::Rights::URWX()),
	    server,
	    L4_fpage::all_spaces(),
	    L4_msg_item(0x1000), &reap);

  assert (ms(server)->v_lookup(to_vaddr(0x1000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));
  assert (phys == Phys_addr(0x10000));
  assert (page_attribs.rights == L4_fpage::Rights::URWX());

  assert (mapdb->lookup(sigma0, to_pfn(0x10000), to_pfn(0x10000), &m, &frame));
  print_node (m, frame);
  mapdb->free (frame);

  cout << "[UTEST] s0 [0/superpage] -> server [0] -> should map many 4K pages and "
          "overmap previous mapping" << endl;

  assert (ms(server)->v_lookup(to_vaddr(0), &phys, &order, &page_attribs)
	  == false);

  fpage_map(sigma0,
	    L4_fpage::mem(0, Config::SUPERPAGE_SHIFT, L4_fpage::Rights::URX()),
	    server,
	    L4_fpage::all_spaces(),
	    L4_msg_item(0), &reap);

  assert (ms(server)->v_lookup(to_vaddr(0), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));	// and not SUPERPAGE_SIZE!
  assert (phys == Phys_addr(0));
  assert (page_attribs.rights == L4_fpage::Rights::URX());

  assert (mapdb->lookup(sigma0, to_pfn(0), to_pfn(0), &m, &frame));
  print_node (m, frame);
  mapdb->free (frame);

  // previous mapping still there?

  assert (ms(server)->v_lookup(to_vaddr(0x1000), &phys, &order, &page_attribs));
  assert (order == Page_order(Config::PAGE_SHIFT));
  // Previously, overmapping did not work and was ignored, so the
  // lookup yielded the previously established mapping:
  //   assert (phys == 0x10000);
  //   assert (page_attribs == (Mem_space::Page_writable | Mem_space::Page_user_accessible));
  // Now, the previous mapping should have been overwritten:
  assert (phys == Phys_addr(0x1000));
  assert (page_attribs.rights == L4_fpage::Rights::URX());

  // mapdb entry -- tree should now contain another mapping 
  // s0 [0x10000] -> server [0x10000]
  assert (mapdb->lookup(sigma0, to_pfn(0x10000), to_pfn(0x10000), &m, &frame));
  print_node (m, frame, 0x10000, 0x11000);
  mapdb->free (frame);

  cout << "[UTEST] Partially unmap superpage s0 [0/superpage]" << endl;

  assert (ms(server)->v_lookup(to_vaddr(0x101000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));
  assert (phys == Phys_addr(0x101000));
  assert (page_attribs.rights == L4_fpage::Rights::URX());

  fpage_unmap(sigma0, 
	      Test_fpage::mem(0x100000, Config::SUPERPAGE_SHIFT - 2, L4_fpage::Rights::URWX()),
	      L4_map_mask(0) /*full unmap, not me too)*/, reap.list());
  
  assert (mapdb->lookup(sigma0, to_pfn(0x0), to_pfn(0x0), &m, &frame));
  print_node (m, frame);
  mapdb->free (frame);

  assert (! ms(server)->v_lookup(to_vaddr(0x101000), &phys, &order, &page_attribs));

  // 
  // s0 [4M/superpage] -> server [8M]
  // 
  assert (ms(server)->v_lookup(to_vaddr(0x800000), &phys, &order, &page_attribs)
	  == false);

  fpage_map(sigma0,
	    Test_fpage::mem(0x400000, Config::SUPERPAGE_SHIFT, L4_fpage::Rights::URWX()),
	    server,
	    L4_fpage::mem(0x800000, Config::SUPERPAGE_SHIFT),
	    L4_msg_item(0), &reap);

  assert (ms(server)->v_lookup(to_vaddr(0x800000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::SUPERPAGE_SHIFT));
  assert (phys == Virt_addr(0x400000));
  assert (page_attribs.rights == L4_fpage::Rights::URWX());

  assert (mapdb->lookup(sigma0, to_pfn(0x400000), to_pfn(0x400000), &m, &frame));
  print_node (m, frame);
  mapdb->free (frame);

  // 
  // server [8M+4K] -> client [8K]
  // 
  assert (ms(client)->v_lookup(to_vaddr(0x8000), &phys, &order, &page_attribs)
	  == false);

  fpage_map(server, 
	    Test_fpage::mem(0x801000, Config::PAGE_SHIFT, L4_fpage::Rights::URWX()),
	    client,
	    L4_fpage::mem(0, L4_fpage::Whole_space),
	    L4_msg_item(0x8000), &reap);

  assert (ms(client)->v_lookup(to_vaddr(0x8000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));
  assert (phys == Virt_addr(0x401000));
  assert (page_attribs.rights == L4_fpage::Rights::URWX());

  // Previously, the 4K submapping is attached to the Sigma0 parent.
  // Not any more.

  assert (mapdb->lookup(sigma0, to_pfn(0x400000), to_pfn(0x400000), &m, &frame));
  print_node (m, frame);
  mapdb->free (frame);

  //
  // Overmap a read-only page.  The writable attribute should not be
  // flushed.
  //
  fpage_map(server,
	    Test_fpage::mem(0x801000, Config::PAGE_SHIFT, L4_fpage::Rights::URX()),
	    client,
	    L4_fpage::mem(0, L4_fpage::Whole_space),
	    L4_msg_item(0x8000), &reap);

  assert (ms(client)->v_lookup(to_vaddr(0x8000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));
  assert (phys == Virt_addr(0x401000));
  assert (page_attribs.rights == L4_fpage::Rights::URWX());

#if 0 // no selective unmap anymore
  // 
  // Try selective unmap
  //
  fpage_map (server,
	     Test_fpage::mem(0x801000, Config::PAGE_SHIFT, L4_fpage::RWX),
	     client2,
	     L4_fpage::all_spaces(),
	     0x1000, &reap);

  assert (client2->mem_space()->v_lookup (0x1000, &phys, &size, &page_attribs)
	  == true);
  assert (size == Config::PAGE_SIZE);
  assert (phys == 0x401000);
  assert (page_attribs == (Mem_space::Page_writable 
			   | Mem_space::Page_user_accessible));  

  assert (mapdb->lookup (sigma0, 0x400000, 0x400000, &m, &frame));
  print_node (m, frame);
  mapdb->free (frame);

  fpage_unmap (server,
	       Test_fpage (false, true, Config::PAGE_SHIFT, 0x801000),
	       mask(false, client2->id(), true, false, false));

  // Page should have vanished in client2's space, but should still be
  // present in client's space.
  assert (client2->mem_space()->v_lookup (0x1000, &phys, &size, &page_attribs)
	  == false);
  assert (client->mem_space()->v_lookup (0x8000, &phys, &size, &page_attribs)
	  == true);
  assert (size == Config::PAGE_SIZE);
  assert (phys == 0x401000);
  assert (page_attribs == (Mem_space::Page_writable 
			   | Mem_space::Page_user_accessible));  

  assert (mapdb->lookup (sigma0->id(), 0x400000, 0x400000, &m, &frame));
  print_node (m, frame);
  mapdb->free (frame);
#endif


  // 
  // Try some Accessed / Dirty flag unmaps
  // 
  
  // touch page in client
  ms(client)->v_set_access_flags(to_vaddr(0x8000), L4_fpage::Rights::RW());

  assert (ms(client)->v_lookup(to_vaddr(0x8000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));
  assert (phys == Virt_addr(0x401000));
  assert (page_attribs.rights == L4_fpage::Rights::URWX());
  // the next would reset flags, so don't do it!!
  // assert (ms(client)->v_delete(to_vaddr(0x8000), order, L4_fpage::Rights(0)) == L4_fpage::Rights::RW());

  // reset dirty from server
  assert (fpage_unmap(server,
		      Test_fpage::mem(0x801000, Config::PAGE_SHIFT),
		      L4_map_mask(0), reap.list()
		      /*no_unmap + reset_refs*/)
	  == L4_fpage::Rights::RW());

  assert (ms(client)->v_lookup(to_vaddr(0x8000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));
  assert (phys == Virt_addr(0x401000));
  assert (ms(client)->v_delete(to_vaddr(0x8000), order, L4_fpage::Rights(0)) == L4_fpage::Rights(0));

  assert (ms(server)->v_lookup(to_vaddr(0x801000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::SUPERPAGE_SHIFT));
  assert (phys == Virt_addr(0x400000));

  // flush dirty and accessed from server
  assert (fpage_unmap(server,
		      Test_fpage::mem(0x800000, Config::SUPERPAGE_SHIFT),
		      L4_map_mask(0x80000000), reap.list())
	  == L4_fpage::Rights::RW());

  assert (ms(client)->v_lookup(to_vaddr(0x8000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::PAGE_SHIFT));
  assert (phys == Virt_addr(0x401000));
  assert (ms(client)->v_delete(to_vaddr(0x8000), order, L4_fpage::Rights(0)) == L4_fpage::Rights(0));

  assert (ms(server)->v_lookup(to_vaddr(0x800000), &phys, &order, &page_attribs)
	  == true);
  assert (order == Page_order(Config::SUPERPAGE_SHIFT));
  assert (phys == Virt_addr(0x400000));
  assert (page_attribs.rights == L4_fpage::Rights::URWX());

  // 
  // Delete tasks
  // 
#if 0
  // do not do this because the mapping database would crash if 
  // they has mappings without spaces
  delete server;
  delete client;
  delete client2;
  delete sigma0;
#endif

  cerr << "OK" << endl;

  return 0;
}


static
std::ostream &operator << (std::ostream &s, Mapdb::Pfn v)
{
  s << cxx::int_value<Mapdb::Pfn>(v);
  return s;
}

static
std::ostream &operator << (std::ostream &s, Space const &sp)
{
  Test_space const *t = static_cast<Test_space const *>(&sp);
  if (!t)
    s << "<NULL>";
  else
    s << t->name;
  return s;
}


static void print_node(Mapping* node, const Mapdb::Frame& frame,
		       Address va_begin = 0, Address va_end = ~0UL)
{
  assert (node);

  Mapdb::Order size = Mapdb::shift(frame, node);

  cout << "space="  << *node->space()
       << " vaddr=0x" << node->pfn(size)
       << " size=0x" << (Mapdb::Pfn(1) << size)
       << endl;

  Mapdb::foreach_mapping(frame, node, to_pfn(va_begin), to_pfn(va_end),
    [](Mapping *node, Mapdb::Order order)
    {
      cout << "[UTEST] ";
      for (int d = node->depth(); d != 0; d--)
        cout << ' ';

      cout << setbase(16)
	   << "space="  << *node->space()
	   << " vaddr=0x" << node->pfn(order)
	   << " size=0x" << (Mapdb::Pfn(1) << order);

      if (Mapping* p = node->parent())
	{
	  cout << " parent=" << *p->space()
	       << " p.vaddr=0x" << p->pfn(order);
	}

      cout << endl;
    });
  cout << "[UTEST] " << endl;
}

