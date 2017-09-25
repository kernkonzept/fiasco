#include <iostream>
#include <iomanip>
#include <cassert>

using namespace std;

#include <flux/page.h>
#include "l4_types.h"
#include "l4_ipc.h"
#include "map_util.h"
#include "space.h"
#include "globals.h"
#include "config.h"

static void print_node(mapping_t* node, int depth = 0);

int main()
{
  // 
  // Create tasks
  // 
  sigma0 = new Space (2);

  Space *server = new Space (5);
  assert (server);
  Space *client = new Space (6);
  assert (client);

  // 
  // Manipulate mappings.
  // 
  mapdb_t* mapdb = &the_mapdb;

  vm_offset_t phys, size;
  unsigned page_attribs;
  mapping_t *m;
  
  // 
  // s0 [0x10000] -> server [0x1000]
  // 
  assert (fpage_map (sigma0, 
		     l4_fpage (0x10000, L4_LOG2_PAGESIZE, L4_FPAGE_RW, 0),
		     server,
		     l4_fpage (0, L4_WHOLE_ADDRESS_SPACE, 0, 0),
		     0x1000) 
	  == L4_IPC_FPAGE_MASK);

  assert (server->v_lookup (0x1000, &phys, &size, &page_attribs) == true);
  assert (size == PAGE_SIZE);
  assert (phys == 0x10000);
  assert (page_attribs == (Space::Page_writable 
			   | Space::Page_user_accessible));

  m = mapdb->lookup (sigma0->space(), 0x10000, 0x10000);
  print_node (m);
  mapdb->free (m);

  // 
  // s0 [0/superpage] -> server [0]  -> should map many 4K pages and skip
  // previos page
  // 
  assert (fpage_map (sigma0, 
		     l4_fpage (0, L4_LOG2_SUPERPAGESIZE, 0, 0),
		     server,
		     l4_fpage (0, L4_WHOLE_ADDRESS_SPACE, 0, 0),
		     0) == L4_IPC_FPAGE_MASK);

  assert (server->v_lookup (0, &phys, &size, &page_attribs) == true);
  assert (size == PAGE_SIZE);	// and not SUPERPAGE_SIZE!
  assert (phys == 0);
  assert (page_attribs == Space::Page_user_accessible);

  m = mapdb->lookup (sigma0->space(), 0, 0);
  print_node (m);
  mapdb->free (m);

  // previous mapping still there?
  assert (server->v_lookup (0x1000, &phys, &size, &page_attribs) == true);
  assert (size == PAGE_SIZE);
  assert (phys == 0x10000);
  assert (page_attribs == (Space::Page_writable 
			   | Space::Page_user_accessible));

  // mapdb entry -- tree should now contain another mapping 
  // s0 [0x10000] -> server [0x10000]
  m = mapdb->lookup (sigma0->space(), 0x10000, 0x10000);
  print_node (m);
  mapdb->free (m);

  // 
  // s0 [4M/superpage] -> server [8M]
  // 
  assert (fpage_map (sigma0, 
		     l4_fpage (0x400000, L4_LOG2_SUPERPAGESIZE, L4_FPAGE_RW,0),
		     server,
		     l4_fpage (0x800000, L4_LOG2_SUPERPAGESIZE, 0, 0),
		     0) == L4_IPC_FPAGE_MASK);

  assert (server->v_lookup (0x800000, &phys, &size, &page_attribs) == true);
  assert (size == SUPERPAGE_SIZE);
  assert (phys == 0x400000);
  assert (page_attribs == (Space::Page_writable 
			   | Space::Page_user_accessible));

  m = mapdb->lookup (sigma0->space(), 0x400000, 0x400000);
  print_node (m);
  mapdb->free (m);

  // 
  // server [8M+4K] -> client [8K]
  // 
  assert (fpage_map (server, 
		     l4_fpage (0x801000, L4_LOG2_PAGESIZE, L4_FPAGE_RW,0),
		     client,
		     l4_fpage (0, L4_WHOLE_ADDRESS_SPACE, 0, 0),
		     0x8000) == L4_IPC_FPAGE_MASK);

  assert (client->v_lookup (0x8000, &phys, &size, &page_attribs) == true);
  assert (size == PAGE_SIZE);
  assert (phys == 0x401000);
  assert (page_attribs == (Space::Page_writable 
			   | Space::Page_user_accessible));

  cout << "XXX: The 4K submapping is attached to the Sigma0 parent." << endl;
  m = mapdb->lookup (sigma0->space(), 0x401000, 0x401000);
  print_node (m);
  mapdb->free (m);

  // 
  // Delete tasks
  // 
  delete server;
  delete client;
  delete sigma0;

  cerr << "OK" << endl;

  return 0;
}

static void print_node(mapping_t* node, int depth = 0)
{
  assert (node);

  for (int i = depth; i != 0; i--)
    cout << ' ';

  cout << setbase(16)
       << "space=0x"  << node->space() 
       << " vaddr=0x" << node->vaddr()
       << " size=0x" << node->size()
       << " type=0x" << node->type();

  unsigned after = 0;
  mapping_t* next = node;
  while ((next = next->next_iter()))
    after++;

  cout << " after=" << after << endl;

  next = node;
  while ((next = next->next_child(node)))
    {
      if(next->parent() == node)
	print_node (next, depth + 1);
    }

  if (depth == 0)
    cout << endl;
}
