INTERFACE [arm]:

#include "types.h"

class Mem_op
{
public:
  enum Op_cache
  {
    Op_cache_clean_data        = 0x00,
    Op_cache_flush_data        = 0x01,
    Op_cache_inv_data          = 0x02,
    Op_cache_coherent          = 0x03,
    Op_cache_dma_coherent      = 0x04,
    Op_cache_dma_coherent_full = 0x05,
    Op_cache_l2_clean          = 0x06,
    Op_cache_l2_flush          = 0x07,
    Op_cache_l2_inv            = 0x08,
  };

  enum Op_mem
  {
    Op_mem_read_data     = 0x10,
    Op_mem_write_data    = 0x11,
  };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "context.h"
#include "entry_frame.h"
#include "globals.h"
#include "mem.h"
#include "mem_space.h"
#include "mem_unit.h"
#include "outer_cache.h"
#include "space.h"
#include "warn.h"

PRIVATE static void
Mem_op::l1_inv_dcache(Address start, Address end)
{
  Mword s = Mem_unit::dcache_line_size();
  Mword m = s - 1;
  if (start & m)
    {
      Mem_unit::flush_dcache((void *)start, (void *)start);
      start += s;
      start &= ~m;
    }
  if (end & m)
    {
      Mem_unit::flush_dcache((void *)end, (void *)end);
      end &= ~m;
    }

  if (start < end)
    Mem_unit::inv_dcache((void *)start, (void *)end);
}

PRIVATE static void
Mem_op::inv_icache(Address start, Address end)
{
  if (Address(end) - Address(start) > 0x2000)
    asm volatile("mcr p15, 0, r0, c7, c5, 0");
  else
    {
      Mword s = Mem_unit::icache_line_size();
      for (start &= ~(s - 1);
           start < end; start += s)
	asm volatile("mcr p15, 0, %0, c7, c5, 1" : : "r" (start));
    }
}

PRIVATE static inline void
Mem_op::__arm_kmem_l1_cache_maint(int op, void const *kstart, void const *kend)
{
  switch (op)
    {
    case Op_cache_clean_data:
      Mem_unit::clean_dcache(kstart, kend);
      break;

    case Op_cache_flush_data:
      Mem_unit::flush_dcache(kstart, kend);
      break;

    case Op_cache_inv_data:
      l1_inv_dcache((Address)kstart, (Address)kend);
      break;

    case Op_cache_coherent:
      Mem_unit::clean_dcache(kstart, kend);
      Mem::dsb();
      Mem_unit::btc_inv();
      inv_icache(Address(kstart), Address(kend));
      Mem::dsb();
      break;

    case Op_cache_dma_coherent:
      Mem_unit::flush_dcache(Virt_addr(Address(kstart)), Virt_addr(Address(kend)));
      break;

    // We might not want to implement this one but single address outer
    // cache flushing can be really slow
    case Op_cache_dma_coherent_full:
      Mem_unit::flush_dcache();
      break;

    default:
      break;
    };
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !hyp]:

PRIVATE static inline void
Mem_op::__arm_mem_l1_cache_maint(int op, void const *start, void const *end)
{ __arm_kmem_l1_cache_maint(op, start, end); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && hyp]:

PRIVATE static inline void
Mem_op::__arm_mem_l1_cache_maint(int op, void const *start, void const *end)
{
  if (op == Op_cache_dma_coherent_full)
    {
      __arm_kmem_l1_cache_maint(Op_cache_dma_coherent_full, 0, 0);
      return;
    }

  Virt_addr v = Virt_addr((Address)start);
  Virt_addr e = Virt_addr((Address)end);

  Context *c = current();

  while (v < e)
    {
      Mem_space::Page_order phys_size;
      Mem_space::Phys_addr phys_addr;
      Page::Attr attrs;
      bool mapped = (   c->mem_space()->v_lookup(Mem_space::Vaddr(v), &phys_addr, &phys_size, &attrs)
                     && (attrs.rights & Page::Rights::U()));

      Virt_size sz = Virt_size(1) << phys_size;
      Virt_size offs = cxx::get_lsb(v, phys_size);
      sz -= offs;
      if (e - v < sz)
        sz = e - v;

      if (mapped)
        {
          Virt_addr vstart = Virt_addr(phys_addr) | offs;
          Virt_addr vend = vstart + sz;
          __arm_kmem_l1_cache_maint(op, (void*)Virt_addr::val(vstart),
                                    (void*)Virt_addr::val(vend));
        }
      v += sz;
    }

}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

PUBLIC static void
Mem_op::arm_mem_cache_maint(int op, void const *start, void const *end)
{
  if (EXPECT_FALSE(start > end))
    return;

  Context *c = current();

  c->set_ignore_mem_op_in_progress(true);
  __arm_mem_l1_cache_maint(op, start, end);
  c->set_ignore_mem_op_in_progress(false);
  switch (op)
    {
    case Op_cache_l2_clean:
    case Op_cache_l2_flush:
    case Op_cache_l2_inv:
      outer_cache_op(op, Address(start), Address(end));
      break;

    case Op_cache_dma_coherent:
      outer_cache_op(Op_cache_l2_flush, Address(start), Address(end));
      break;

    // We might not want to implement this one but single address outer
    // cache flushing can be really slow
    case Op_cache_dma_coherent_full:
      Outer_cache::flush();
      break;

    default:
      break;
    };

}

PUBLIC static void
Mem_op::arm_mem_access(Mword *r)
{
  Address  a = r[1];
  unsigned w = r[2];

  if (w > 2)
    return;

  if (!current()->space()->is_user_memory(a, 1 << w))
    return;

  jmp_buf pf_recovery;
  int e;

  if ((e = setjmp(pf_recovery)) == 0)
    {
      current()->recover_jmp_buf(&pf_recovery);

      switch (r[0])
	{
	case Op_mem_read_data:
	  switch (w)
	    {
	    case 0:
	      r[3] = *(unsigned char *)a;
	      break;
	    case 1:
	      r[3] = *(unsigned short *)a;
	      break;
	    case 2:
	      r[3] = *(unsigned int *)a;
	      break;
	    default:
	      break;
	    };
	  break;

	case Op_mem_write_data:
	  switch (w)
	    {
	    case 0:
	      *(unsigned char *)a = r[3];
	      break;
	    case 1:
	      *(unsigned short *)a = r[3];
	      break;
	    case 2:
	      *(unsigned int *)a = r[3];
	      break;
	    default:
	      break;
	    };
	  break;

	default:
	  break;
	};
    }
  else
    WARN("Unresolved memory access, skipping\n");

  current()->recover_jmp_buf(0);
}

extern "C" void sys_arm_mem_op()
{
  Entry_frame *e = current()->regs();
  if (EXPECT_FALSE(e->r[0] & 0x10))
    Mem_op::arm_mem_access(e->r);
  else
    Mem_op::arm_mem_cache_maint(e->r[0], (void *)e->r[1], (void *)e->r[2]);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !outer_cache]:

PRIVATE static inline
void
Mem_op::outer_cache_op(int, Address, Address)
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && outer_cache]:

PRIVATE static
void
Mem_op::outer_cache_op(int op, Address start, Address end)
{

  Virt_addr v = Virt_addr(start);
  Virt_addr e = Virt_addr(end);

  Context *c = current();

  while (v < e)
    {
      Mem_space::Page_order phys_size;
      Mem_space::Phys_addr phys_addr;
      Page::Attr attrs;
      bool mapped = (   c->mem_space()->v_lookup(Mem_space::Vaddr(v), &phys_addr, &phys_size, &attrs)
                     && (attrs.rights & Page::Rights::U()));

      Virt_size sz = Virt_size(1) << phys_size;
      Virt_size offs = cxx::get_lsb(v, phys_size);
      sz -= offs;
      if (e - v < sz)
        sz = e - v;

      if (mapped)
        {
          Virt_addr vstart = Virt_addr(phys_addr) | offs;
          Virt_addr vend = vstart + sz;
          switch (op)
            {
            case Op_cache_l2_clean:
              Outer_cache::clean(Virt_addr::val(vstart), Virt_addr::val(vend), false);
              break;
            case Op_cache_l2_flush:
              Outer_cache::flush(Virt_addr::val(vstart), Virt_addr::val(vend), false);
              break;
            case Op_cache_l2_inv:
              Outer_cache::invalidate(Virt_addr::val(vstart), Virt_addr::val(vend), false);
              break;
            }
        }
      v += sz;
    }
  Outer_cache::sync();
}
