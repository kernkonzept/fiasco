INTERFACE [vmx]:

#include "vm_vmx.h"
#include "ptab_base.h"
#include "tlbs.h"

class Vm_vmx_ept_tlb : public Tlb
{
public:
  enum Context
  {
    Single = 0x1,
    Global = 0x2,
  };

  static void invept(Context type, Mword eptp = 0)
  {
    struct Op
    {
      Unsigned64 eptp, resv;
    } op = {eptp, 0};

    asm volatile (
      "invept %[op], %[type]\n"
      :
      : [op] "m" (op),
        [type] "r" (static_cast<Mword>(type))
      : "cc"
    );
  }

  /**
   * Flush only the TLB entries corresponding to the given EPTP.
   */
  static void flush_single(Mword eptp)
  {
    if (Vmx::cpus.current().info.has_invept_single())
      invept(Single, eptp);
    else
      invept(Global);
  }

  /**
   * Flush the complete EPT TLB.
   */
  static void flush()
  {
    invept(Global);
  }

  explicit Vm_vmx_ept_tlb(Cpu_number cpu)
  {
    auto const &vmx = Vmx::cpus.cpu(cpu);
    if (!vmx.info.procbased_ctls2.allowed(Vmx_info::PRB2_enable_ept))
      // EPT is not enabled, no need for EPT TLB maintenance.
      return;

    // Clean out residual EPT TLB entries during boot.
    Vm_vmx_ept_tlb::flush();

    register_cpu_tlb(cpu);
  }

  inline void tlb_flush() override
  {
    Vm_vmx_ept_tlb::flush();
  }

private:
  static Per_cpu<Vm_vmx_ept_tlb> cpu_tlbs;
};

/**
 * VMX implementation variant with EPT support.
 */
class Vm_vmx_ept :
  public cxx::Dyn_castable<Vm_vmx_ept, Vm_vmx_t<Vm_vmx_ept>>
{
private:
  class Epte_ptr
  {
    Unsigned64 *e;

    void set(Unsigned64 v);

  public:
    typedef Mem_space::Attr Attr;

    unsigned char level;

    Epte_ptr() = default;
    Epte_ptr(Unsigned64 *e, unsigned char level) : e(e), level(level) {}

    bool is_valid() const { return *e & 7; }
    bool is_leaf() const { return (*e & (1 << 7)) || level == 3; }
    Unsigned64 next_level() const
    { return cxx::get_lsb(cxx::mask_lsb(*e, 12), 52); }

    void clear() { set(0); }

    unsigned page_level() const;
    unsigned char page_order() const;
    Unsigned64 page_addr() const;

    Attr attribs() const
    {
      auto raw = access_once(e);

      Page::Rights r = Page::Rights::UR();
      if (raw & 2) r |= Page::Rights::W();
      if (raw & 4) r |= Page::Rights::X();

      Page::Type t;
      switch (raw & 0x38)
        {
        case (0 << 3): t = Page::Type::Uncached(); break;
        case (1 << 3): t = Page::Type::Buffered(); break;
        default:
        case (6 << 3): t = Page::Type::Normal(); break;
        }

      return Attr(r, t, Page::Kern::None(), Page::Flags::None());
    }

    Unsigned64 entry() const { return *e; }

    void set_next_level(Unsigned64 phys)
    { set(phys | 7); }

    void write_back_if(bool) const {}
    static void write_back(void *, void*) {}

    Page::Flags access_flags() const
    {
      return Page::Flags::None();
    }

    void del_rights(Page::Rights r)
    {
      Unsigned64 dr = 0;

      if (r & Page::Rights::W())
        dr = 2;

      if (r & Page::Rights::X())
        dr |= 4;

      if (dr)
        *e &= ~dr;
    }

    Unsigned64 make_page(Phys_mem_addr addr, Page::Attr attr)
    {
      Unsigned64 r = (level < 3) ? 1ULL << 7 : 0ULL;
      r |= 1; // R
      if (attr.rights & Page::Rights::W()) r |= 2;
      if (attr.rights & Page::Rights::X()) r |= 4;

      if (attr.type == Page::Type::Normal())   r |= 6 << 3;
      if (attr.type == Page::Type::Buffered()) r |= 1 << 3;
      if (attr.type == Page::Type::Uncached()) r |= 0;

      return cxx::int_value<Phys_mem_addr>(addr) | r;
    }

    void set_page(Unsigned64 p)
    {
      set(p);
    }
  };

  typedef Ptab::Tupel<Ptab::Traits<Unsigned64, 39, 9, false>,
                      Ptab::Traits<Unsigned64, 30, 9, true>,
                      Ptab::Traits<Unsigned64, 21, 9, true>,
                      Ptab::Traits<Unsigned64, 12, 9, true>>::List Ept_traits;

  typedef Ptab::Shift<Ept_traits, 12>::List Ept_traits_vpn;
  typedef Ptab::Page_addr_wrap<Page_number, 12> Ept_va_vpn;
  typedef Ptab::Base<Epte_ptr, Ept_traits_vpn, Ept_va_vpn, Mem_layout> Ept;

  Mword _eptp;
  Ept *_ept;
};

// -------------------------------------------------------------------------
IMPLEMENTATION [obj_space_virt]:

// avoid warnings about hiding virtual functions from Obj_space_phys_override
EXTENSION class Vm_vmx_ept
{
private:
  using Vm_vmx_t<Vm_vmx_ept>::v_lookup;
  using Vm_vmx_t<Vm_vmx_ept>::v_delete;
  using Vm_vmx_t<Vm_vmx_ept>::v_insert;
};

// -------------------------------------------------------------------------
IMPLEMENTATION [vmx && 64bit]:

IMPLEMENT inline
void
Vm_vmx_ept::Epte_ptr::set(Unsigned64 v) { write_now(e, v); }

// -------------------------------------------------------------------------
IMPLEMENTATION [vmx && !64bit]:

IMPLEMENT inline
void
Vm_vmx_ept::Epte_ptr::set(Unsigned64 v)
{
  // this assumes little endian!
  union T
  {
    Unsigned64 l;
    Unsigned32 u[2];
  };

  T *t = reinterpret_cast<T*>(e);

  write_now(&t->u[0], 0U);
  write_now(&t->u[1], static_cast<Unsigned32>(v >> 32));
  write_now(&t->u[0], static_cast<Unsigned32>(v));
}

// -------------------------------------------------------------------------
IMPLEMENTATION [vmx]:

static Kmem_slab_t<Vm_vmx_ept> _ept_allocator("Vm_vmx_ept");

IMPLEMENT inline
unsigned
Vm_vmx_ept::Epte_ptr::page_level() const
{ return level; }

IMPLEMENT inline
unsigned char
Vm_vmx_ept::Epte_ptr::page_order() const
{ return Vm_vmx_ept::Ept::page_order_for_level(level); }

IMPLEMENT inline
Unsigned64
Vm_vmx_ept::Epte_ptr::page_addr() const
{ return cxx::get_lsb(cxx::mask_lsb(*e, Vm_vmx_ept::Ept::page_order_for_level(level)), 52); }

static Mem_space::Fit_size __ept_ps;

PUBLIC
Mem_space::Fit_size const &
Vm_vmx_ept::mem_space_fitting_sizes() const override
{ return __ept_ps; }

PUBLIC static
void
Vm_vmx_ept::add_page_size(Mem_space::Page_order o)
{
  add_global_page_size(o);
  __ept_ps.add_page_size(o);
}

PUBLIC
void
Vm_vmx_ept::tlb_flush_current_cpu() override
{
  Vm_vmx_ept_tlb::flush_single(_eptp);
  tlb_mark_unused();
}

PUBLIC
bool
Vm_vmx_ept::v_lookup(Mem_space::Vaddr virt, Mem_space::Phys_addr *phys,
                     Mem_space::Page_order *order,
                     Mem_space::Attr *page_attribs) override
{
  auto i = _ept->walk(virt);
  if (order) *order = Mem_space::Page_order(i.page_order());

  if (!i.is_valid())
    return false;

  // FIXME: we may get a problem on 32 bit systems and using more than 4G ram
  if (phys) *phys = Mem_space::Phys_addr(i.page_addr());
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

PUBLIC
Mem_space::Status
Vm_vmx_ept::v_insert(Mem_space::Phys_addr phys, Mem_space::Vaddr virt,
                     Mem_space::Page_order order,
                     Mem_space::Attr page_attribs, bool) override
{
  // insert page into page table

  // XXX should modify page table using compare-and-swap

  assert(cxx::is_zero(cxx::get_lsb(Mem_space::Phys_addr(phys), order)));
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  int level;
  for (level = 0; level <= Ept::Depth; ++level)
    if (Mem_space::Page_order(Ept::page_order_for_level(level)) <= order)
      break;

  auto i = _ept->walk(virt, level, false,
                      Kmem_alloc::q_allocator(ram_quota()));

  if (EXPECT_FALSE(!i.is_valid() && i.level != level))
    return Mem_space::Insert_err_nomem;

  if (EXPECT_FALSE(i.is_valid()
                   && (i.level != level || Mem_space::Phys_addr(i.page_addr()) != phys)))
    return Mem_space::Insert_err_exists;

  auto entry = i.make_page(phys, page_attribs);

  if (i.is_valid())
    {
      if (EXPECT_FALSE(i.entry() == entry))
        return Mem_space::Insert_warn_exists;

      i.set_page(entry);
      return Mem_space::Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      return Mem_space::Insert_ok;
    }
}

PUBLIC
Page::Flags
Vm_vmx_ept::v_delete(Mem_space::Vaddr virt,
                     [[maybe_unused]] Mem_space::Page_order order,
                     Page::Rights rights) override
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto pte = _ept->walk(virt);

  if (EXPECT_FALSE(!pte.is_valid()))
    return Page::Flags::None();

  Page::Flags flags = pte.access_flags();

  if (!(rights & Page::Rights::R()))
    {
      // downgrade PDE (superpage) rights
      pte.del_rights(rights);
    }
  else
    {
      // delete PDE (superpage)
      pte.clear();
    }

  return flags;
}

PUBLIC
void
Vm_vmx_ept::v_add_access_flags(Mem_space::Vaddr, Page::Flags) override
{}

PUBLIC static
Vm_vmx_ept *Vm_vmx_ept::alloc(Ram_quota *q)
{
  return _ept_allocator.q_new(q, q);
}

PUBLIC inline
void *
Vm_vmx_ept::operator new (size_t size, void *p) noexcept
{
  (void)size;
  assert(size == sizeof(Vm_vmx_ept));
  return p;
}

PUBLIC
void
Vm_vmx_ept::operator delete (Vm_vmx_ept *vm, std::destroying_delete_t)
{
  Ram_quota *q = vm->ram_quota();
  vm->~Vm_vmx_ept();
  _ept_allocator.q_free(q, vm);
}

PUBLIC inline
Vm_vmx_ept::Vm_vmx_ept(Ram_quota *q) : Dyn_castable_class(q)
{
  _tlb_type = Tlb_per_cpu_asid;
}

PUBLIC
Vm_vmx_ept::~Vm_vmx_ept()
{
  if (_ept)
    {
      _ept->destroy(Virt_addr(0UL), Virt_addr(~0UL), 0, Ept::Depth,
                    Kmem_alloc::q_allocator(ram_quota()));
      Kmem_alloc::allocator()->q_free(ram_quota(), Config::page_order(), _ept);
      _ept = nullptr;
      _eptp = 0;
    }
}

PUBLIC inline
void
Vm_vmx_ept::to_vmcs()
{
  Vmx::vmcs_write<Vmx::Vmcs_ept_pointer>(_eptp);
  tlb_mark_used();
}

PUBLIC inline
bool
Vm_vmx_ept::initialize()
{
  void *b;
  if (EXPECT_FALSE(!(b = Kmem_alloc::allocator()
	  ->q_alloc(ram_quota(), Config::page_order()))))
    return false;

  _ept = static_cast<Ept*>(b);
  _ept->clear(false);	// initialize to zero
  _eptp = Mem_layout::pmem_to_phys(_ept) | 6 | (3 << 3);
  return true; // success

}

PUBLIC inline
void
Vm_vmx_ept::load_vm_memory(Vmx_vm_state *vm_state)
{
  vm_state->load_cr3();
  to_vmcs();
}

PUBLIC inline
void
Vm_vmx_ept::store_vm_memory(Vmx_vm_state *vm_state)
{
  vm_state->store_cr3();
}

PUBLIC static
void
Vm_vmx_ept::init()
{
  auto const &vmx = Vmx::cpus.cpu(Cpu_number::boot_cpu());
  if (!vmx.vmx_enabled())
    return;

  if (!vmx.info.procbased_ctls2.allowed(Vmx_info::PRB2_enable_ept))
    {
      Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                                 Task::generic_factory<Vm_vmx>);
      return;
    }

  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             Task::generic_factory<Vm_vmx_ept>);

  printf("VMX: init page sizes\n");
  add_page_size(Mem_space::Page_order(12));
  add_page_size(Mem_space::Page_order(21));
  add_page_size(Mem_space::Page_order(30));
}

STATIC_INITIALIZE(Vm_vmx_ept);

// -------------------------------------------------------------------------
IMPLEMENTATION[vmx]:

// Must be constructed after Vmx::cpus.
DEFINE_PER_CPU_LATE Per_cpu<Vm_vmx_ept_tlb>
  Vm_vmx_ept_tlb::cpu_tlbs(Per_cpu_data::Cpu_num);
