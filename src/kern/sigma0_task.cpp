INTERFACE:

#include "task.h"

class Sigma0_task : public Task
{
public:
  explicit Sigma0_task(Ram_quota *q) : Task(q) {}
  bool is_sigma0() const override { return true; }
  Address virt_to_phys_s0(void *virt) const override
  { return (Address)virt; }
};


IMPLEMENTATION:

#include "per_node_data.h"

PUBLIC
bool
Sigma0_task::v_fabricate(Mem_space::Vaddr address,
                         Mem_space::Phys_addr *phys, Mem_space::Page_order *size,
                         Mem_space::Attr *attribs = 0) override
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
Sigma0_task::mem_space_map_max_address() const override
{ return Page_number(1UL << (MWORD_BITS - Mem_space::Page_shift)); }

static DECLARE_PER_NODE Per_node_data<Static_object<Sigma0_task>> sigma0_task;

PUBLIC static
Sigma0_task *Sigma0_task::alloc(Ram_quota *q)
{
  sigma0_task->construct(q);
  return sigma0_task->get();
}
