#include <iostream>
#include <iomanip>
#include <cassert>

using namespace std;

#include <flux/page.h>
#include "space.h"

int main()
{
  // 
  // Make task 4 the chief of task 5
  // 
  assert (space_index_t (5).lookup () == 0);
  assert (space_index_t (5).chief () == 0);

  assert (space_index_t (5).set_chief (space_index_t(0), space_index_t(4)));

  assert (space_index_t (5).lookup () == 0);
  assert (space_index_t (5).chief () == 4);

  // 
  // Create task 5.  Make sure it is registered correctly.
  // 
  space_t *s = new space_t (5);

  assert (s);
  assert (space_index_t (5).lookup () == s);
  assert (s->space() == 5);
  assert (s->chief() == 4);

  // 
  // Create a 4K mapping, try flushing attributes
  // 
  vm_offset_t phys, size;
  unsigned attribs;
  assert (s->v_lookup (0xf000, &phys, &size, &attribs) == false);

  assert (s->v_insert (0x1000, 0xf000, PAGE_SIZE,  
		       space_t::Page_user_accessible|space_t::Page_writable)
	  == space_t::Insert_ok);

  assert (s->v_lookup (0xf000, &phys, &size, &attribs) == true);
  assert (phys == 0x1000);
  assert (size == PAGE_SIZE);
  assert (attribs == (space_t::Page_user_accessible|space_t::Page_writable));

  // Flush write permission
  assert (s->v_delete (0xf000, PAGE_SIZE, space_t::Page_writable));
	  
  assert (s->v_lookup (0xf000, &phys, &size, &attribs) == true);
  assert (phys == 0x1000);
  assert (size == PAGE_SIZE);
  assert (attribs == space_t::Page_user_accessible);

  // Remap write permission
  assert (s->v_insert (0x1000, 0xf000, PAGE_SIZE, 
		       space_t::Page_user_accessible|space_t::Page_writable) 
	  == space_t::Insert_warn_attrib_upgrade);

  assert (s->v_lookup (0xf000, &phys, &size, &attribs) == true);
  assert (phys == 0x1000);
  assert (size == PAGE_SIZE);
  assert (attribs == (space_t::Page_user_accessible|space_t::Page_writable));

  // Flush mapping
  assert (s->v_delete (0xf000, PAGE_SIZE));
	  
  assert (s->v_lookup (0xf000, &phys, &size, &attribs) == false);

  // 
  // Try superpages
  // 

  // We expect a superpage mapping in this region to fail because a
  // 2nd-level page table exists (even though we emptied it again in
  // the previous step).  This assertion could be wrong if a more
  // clever space_t implementation was used.
  assert (s->v_lookup (0x0, &phys, &size, &attribs) == false);
  assert (size == PAGE_SIZE);

  assert (s->v_insert (0x800000, 0x0, SUPERPAGE_SIZE, 
		       space_t::Page_user_accessible|space_t::Page_writable)
	  == space_t::Insert_err_exists);

  assert (s->v_lookup (0x0, &phys, &size, &attribs) == false);
  assert (size == PAGE_SIZE);

  // Try a clear entry
  assert (s->v_lookup (0x400000, &phys, &size, &attribs) == false);
  assert (size == SUPERPAGE_SIZE);

  assert (s->v_insert (0x800000, 0x400000, SUPERPAGE_SIZE, 
		       space_t::Page_user_accessible|space_t::Page_writable)
	  == space_t::Insert_ok);

  assert (s->v_lookup (0x400000, &phys, &size, &attribs) == true);
  assert (phys == 0x800000);
  assert (size == SUPERPAGE_SIZE);
  assert (attribs == (space_t::Page_user_accessible|space_t::Page_writable));

  // Flush write permission
  assert (s->v_delete (0x400000, SUPERPAGE_SIZE, space_t::Page_writable));
	  
  assert (s->v_lookup (0x400000, &phys, &size, &attribs) == true);
  assert (phys == 0x800000);
  assert (size == SUPERPAGE_SIZE);
  assert (attribs == space_t::Page_user_accessible);

  // Remap write permission
  assert (s->v_insert (0x800000, 0x400000, SUPERPAGE_SIZE, 
		       space_t::Page_user_accessible|space_t::Page_writable) 
	  == space_t::Insert_warn_attrib_upgrade);

  assert (s->v_lookup (0x400000, &phys, &size, &attribs) == true);
  assert (phys == 0x800000);
  assert (size == SUPERPAGE_SIZE);
  assert (attribs == (space_t::Page_user_accessible|space_t::Page_writable));

  // Flush mapping
  assert (s->v_delete (0x400000, SUPERPAGE_SIZE));
	  
  assert (s->v_lookup (0x400000, &phys, &size, &attribs) == false);

  // 
  // Delete task 5; ensure it is unregistered.
  // 
  delete s;
  assert (space_index_t (5).lookup () == 0);

  cerr << "OK" << endl;

  return 0;
}

