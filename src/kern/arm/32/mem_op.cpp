INTERFACE [arm]:

#include "types.h"
#include "mem.h"

class Mem_op
{
public:
  enum class Op_cache : Mword
  {
    Clean_data        = 0x00,
    Flush_data        = 0x01,
    Inv_data          = 0x02,
    Coherent          = 0x03,
    Dma_coherent      = 0x04,
    Dma_coherent_full = 0x05,
  };

  enum class Op_mem : Mword
  {
    Read_data     = 0x10,
    Write_data    = 0x11,
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

PRIVATE static inline void
Mem_op::__arm_kmem_cache_maint(Op_cache op, void const *kstart, void const *kend)
{
  Address kstart_addr = reinterpret_cast<Address>(kstart);
  Address kend_addr = reinterpret_cast<Address>(kstart);
  switch (op)
    {
    case Op_cache::Clean_data:
      Mem_unit::clean_dcache(kstart, kend);
      Mem::barrier();
      outer_cache_op(kstart_addr, kend_addr,
                     [](Address s, Address e, bool sync)
                     { Outer_cache::clean(s, e, sync); });
      break;

    case Op_cache::Flush_data:
    case Op_cache::Inv_data:
      Mem_unit::flush_dcache(kstart, kend);
      Mem::barrier();
      outer_cache_op(kstart_addr, kend_addr,
                     [](Address s, Address e, bool sync)
                     { Outer_cache::flush(s, e, sync); });
      break;

    case Op_cache::Coherent:
      Mem_unit::clean_dcache(kstart, kend);
      // Our outer cache model assumes a unified outer cache, so there is no
      // need to clean it in order to achieve cache coherency
      Mem::dsb();
      Mem_unit::btc_inv();
      inv_icache(kstart_addr, kend_addr);
      Mem::dsb();
      break;

    case Op_cache::Dma_coherent:
      Mem_unit::flush_dcache(kstart, kend);
      Mem::barrier();
      outer_cache_op(kstart_addr, kend_addr,
                     [](Address s, Address e, bool sync)
                     { Outer_cache::flush(s, e, sync); });
      break;

    // We might not want to implement this one but single address outer
    // cache flushing can be really slow
    case Op_cache::Dma_coherent_full:
      Mem_unit::flush_dcache();
      Mem::barrier();
      Outer_cache::flush();
      break;

    default:
      break;
    };
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

PRIVATE static inline void
Mem_op::__arm_mem_cache_maint(Op_cache op, void const *start, void const *end)
{ __arm_kmem_cache_maint(op, start, end); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

PRIVATE static inline void
Mem_op::__arm_mem_cache_maint(Op_cache op, void const *start, void const *end)
{
  if (op == Op_cache::Dma_coherent_full)
    {
      __arm_kmem_cache_maint(Op_cache::Dma_coherent_full, nullptr, nullptr);
      return;
    }

  Virt_addr v = Virt_addr(reinterpret_cast<Address>(start));
  Virt_addr e = Virt_addr(reinterpret_cast<Address>(end));

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
          __arm_kmem_cache_maint(op, static_cast<void *>(vstart),
                                 static_cast<void *>(vend));
        }
      v += sz;
    }

}

extern "C" void sys_arm_mem_op()
{
  Entry_frame *e = current()->regs();
  Mem_op::arm_mem_cache_maint(Mem_op::Op_cache{e->r[0]},
                              reinterpret_cast<void *>(e->r[1]),
                              reinterpret_cast<void *>(e->r[2]));
}


// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

PUBLIC static void
Mem_op::arm_mem_cache_maint(Op_cache op, void const *start, void const *end)
{
  if (EXPECT_FALSE(start > end))
    return;

  Context *c = current();

  c->set_ignore_mem_op_in_progress(true);
  __arm_mem_cache_maint(op, start, end);
  c->set_ignore_mem_op_in_progress(false);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

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
      current()->set_recover_jmpbuf(&pf_recovery);

      switch (Op_mem{r[0]})
	{
	case Op_mem::Read_data:
	  switch (w)
	    {
	    case 0: r[3] = *reinterpret_cast<unsigned char *>(a); break;
	    case 1: r[3] = *reinterpret_cast<unsigned short *>(a); break;
	    case 2: r[3] = *reinterpret_cast<unsigned int *>(a); break;
	    default: break;
	    };
	  break;

	case Op_mem::Write_data:
	  switch (w)
	    {
	    case 0: *reinterpret_cast<unsigned char *>(a) = r[3]; break;
	    case 1: *reinterpret_cast<unsigned short *>(a) = r[3]; break;
	    case 2: *reinterpret_cast<unsigned int *>(a) = r[3]; break;
	    default: break;
	    };
	  break;

	default:
	  break;
	};
    }
  else
    WARN("Unresolved memory access, skipping\n");

  current()->clear_recover_jmpbuf();
}

extern "C" void sys_arm_mem_op()
{
  Entry_frame *e = current()->regs();
  if (EXPECT_FALSE(e->r[0] & 0x10))
    Mem_op::arm_mem_access(e->r);
  else
    Mem_op::arm_mem_cache_maint(Mem_op::Op_cache{e->r[0]},
                                reinterpret_cast<void *>(e->r[1]),
                                reinterpret_cast<void *>(e->r[2]));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !outer_cache]:

PRIVATE template<typename FUNC> static inline
void
Mem_op::outer_cache_op(Address, Address, FUNC &&)
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && outer_cache]:

PRIVATE template<typename FUNC> static
void
Mem_op::outer_cache_op(Address start, Address end, FUNC &&f)
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
          f(cxx::int_value<Virt_addr>(vstart), cxx::int_value<Virt_addr>(vend), false);
        }
      v += sz;
    }
  Outer_cache::sync();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit]:

PRIVATE static void
Mem_op::inv_icache(Address start, Address end)
{
  if (end - start > 0x2000)
    asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));
  else
    {
      Mword s = Mem_unit::icache_line_size();
      for (start &= ~(s - 1);
           start < end; start += s)
        asm volatile("mcr p15, 0, %0, c7, c5, 1" : : "r" (start));
    }
}
