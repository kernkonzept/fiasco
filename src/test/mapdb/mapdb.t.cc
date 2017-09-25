#include <iostream>
#include <iomanip>
#include <cassert>

using namespace std;

#include <flux/page.h>
#include "config.h"
#include "mapdb.h"

static void print_node(mapping_t* node, int depth = 0);
static void print_whole_tree(mapping_t *node);

static void print_node(mapping_t* node, int depth)
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

void basic()
{
  mapdb_t m (64*1024L*1024L);

  unsigned sigma0 = Config::sigma0_taskno;
  unsigned other  = 10;
  unsigned client = 11;

  mapping_t *node, *sub, *subsub;

  assert (m.lookup(other, PAGE_SIZE, PAGE_SIZE) == 0);

  cout << "Looking up 4K node at physaddr=4K" << endl;
  node = m.lookup (sigma0, PAGE_SIZE, PAGE_SIZE);
  print_node (node);

  cout << "Inserting submapping" << endl;
  sub = m.insert (node, other, 2*PAGE_SIZE, PAGE_SIZE, Map_mem);
  print_node (node);

  m.free (node);

  //////////////////////////////////////////////////////////////////////

  cout << "Looking up 4M node at physaddr=8M" << endl;
  node = m.lookup (sigma0, 2*SUPERPAGE_SIZE, 2*SUPERPAGE_SIZE);
  print_node (node);

  // XXX broken mapdb: assert (node->size() == SUPERPAGE_SIZE);

  cout << "Inserting submapping" << endl;
  sub = m.insert (node, other, 4*SUPERPAGE_SIZE, SUPERPAGE_SIZE, Map_mem);
  print_node (node);

  assert (sub->size() == SUPERPAGE_SIZE);

  // Before we can insert new mappings, we must free the tree.
  m.free (node);

  cout << "Get that mapping again" << endl;
  sub = m.lookup (other, 4*SUPERPAGE_SIZE, 2*SUPERPAGE_SIZE);
  print_node (sub);

  node = sub->parent();

  cout << "Inserting 4K submapping" << endl;
  subsub = m.insert (sub, client, 15*PAGE_SIZE, PAGE_SIZE, Map_mem);
  print_node (node);

  m.free (subsub);		// It doesn't matter which mapping is
				// passed to free().

}

static void print_whole_tree(mapping_t *node)
{
  while(node->parent())
    node = node->parent();
  print_node(node);
}


void maphole()
{
  mapdb_t m(PAGE_SIZE);

  unsigned sigma0 = Config::sigma0_taskno;
  
  mapping_t *gf_map, *f_map, *son_map, *daughter_map, *a_map;

  unsigned grandfather = sigma0;
  unsigned father = 6, son = 7, daughter = 8, aunt = 9;

  cout << "Looking up 4K node at physaddr=0" << endl;
  gf_map = m.lookup (grandfather, 0, 0);
  print_whole_tree (gf_map);

  cout << "Inserting father mapping" << endl;
  f_map = m.insert (gf_map, father, 0, PAGE_SIZE, Map_mem);
  print_whole_tree (f_map);
  m.free(f_map);


  cout << "Looking up father at physaddr=0" << endl;
  f_map = m.lookup (father, 0, 0);
  print_whole_tree (f_map);

  cout << "Inserting son mapping" << endl;
  son_map = m.insert (f_map, son, 0, PAGE_SIZE, Map_mem);
  print_whole_tree (son_map);
  m.free(son_map);


  cout << "Looking up father at physaddr=0" << endl;
  f_map = m.lookup (father, 0, 0);
  print_whole_tree (f_map);

  cout << "Inserting daughter mapping" << endl;
  daughter_map = m.insert (f_map, daughter, 0, PAGE_SIZE, Map_mem);
  print_whole_tree (daughter_map);
  m.free(daughter_map);


  cout << "Looking up son at physaddr=0" << endl;
  son_map = m.lookup(son, 0,0);
  f_map = son_map->parent();
  print_whole_tree (son_map);

  cout << "Son has accident on return from disco" << endl;
  m.flush(son_map, true);
  m.free(f_map);

  cout << "Lost aunt returns from holidays" << endl;
  gf_map = m.lookup (grandfather, 0, 0);
  print_whole_tree (gf_map);

  cout << "Inserting aunt mapping" << endl;
  a_map = m.insert (gf_map, aunt, 0, PAGE_SIZE, Map_mem);
  print_whole_tree (a_map);
  m.free(a_map);

  cout << "Looking up daughter at physaddr=0" << endl;
  daughter_map = m.lookup(daughter, 0,0);
  print_whole_tree (daughter_map);
  f_map = daughter_map->parent();
  cout << "Father of daugther is " << f_map->space() << endl;

  assert(f_map->space() == father);

  m.free(f_map);
}


void flushtest()
{
  mapdb_t m(PAGE_SIZE);

  unsigned sigma0 = Config::sigma0_taskno;
  
  mapping_t *gf_map, *f_map, *son_map, *a_map;

  unsigned grandfather = sigma0;
  unsigned father = 6, son = 7, aunt = 9;

  cout << "Looking up 4K node at physaddr=0" << endl;
  gf_map = m.lookup (grandfather, 0, 0);
  print_whole_tree (gf_map);

  cout << "Inserting father mapping" << endl;
  f_map = m.insert (gf_map, father, 0, PAGE_SIZE, Map_mem);
  print_whole_tree (f_map);
  m.free(f_map);


  cout << "Looking up father at physaddr=0" << endl;
  f_map = m.lookup (father, 0, 0);
  print_whole_tree (f_map);

  cout << "Inserting son mapping" << endl;
  son_map = m.insert (f_map, son, 0, PAGE_SIZE, Map_mem);
  print_whole_tree (son_map);
  m.free(son_map);

  cout << "Lost aunt returns from holidays" << endl;
  gf_map = m.lookup (grandfather, 0, 0);
  print_whole_tree (gf_map);

  cout << "Inserting aunt mapping" << endl;
  a_map = m.insert (gf_map, aunt, 0, PAGE_SIZE, Map_mem);
  print_whole_tree (a_map);
  m.free(a_map);

  cout << "Looking up father at physaddr=0" << endl;
  f_map = m.lookup(father, 0,0);
  gf_map = f_map->parent();
  print_whole_tree (gf_map);

  cout << "father is killed by his new love" << endl;
  m.flush(f_map, true);
  print_whole_tree (gf_map);
  m.free(f_map);

  cout << "Try resurrecting the killed father again" << endl;
  f_map = m.lookup(father, 0,0);
  assert (! f_map);

  cout << "Resurrection is impossible, as it ought to be." << endl;
}



int main()
{
  cout << "Basic test" << endl;
  basic();
  cout << "########################################" << endl;

  cout << "Hole test" << endl;
  maphole();
  cout << "########################################" << endl;

  cout << "Flush test" << endl;
  flushtest();
  cout << "########################################" << endl;

  cerr << "OK" << endl;
  return(0);
}
